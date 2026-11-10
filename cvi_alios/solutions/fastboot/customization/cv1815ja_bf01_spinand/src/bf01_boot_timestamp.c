#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <aos/kernel.h>
#include <drv/cvi_irq.h>
#include <drv/tick.h>
#include <mmio.h>
#include "bf01_boot_timestamp.h"
#include "bf01_boot_timestamp_config.h"
#include "cvi_ipcm.h"
#include "cvi_region.h"
#include "hal_rtc.h"

#define TIMESTAMP_HANDLE 96
#define TIMESTAMP_WIDTH 384U
#define TIMESTAMP_HEIGHT 32U
#define TIMESTAMP_FONT_SCALE 3
#define TIMESTAMP_CHAR_WIDTH 18
#define TIMESTAMP_POLL_MS 100U

enum timestamp_date_format {
    TIMESTAMP_DATE_YMD,
    TIMESTAMP_DATE_MDY,
    TIMESTAMP_DATE_DMY
};

static const uint8_t timestamp_font[][7] = {
    {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},
    {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 1, 14},
    {0, 0, 0, 31, 0, 0, 0},
    {0, 4, 4, 0, 4, 4, 0},
    {1, 2, 2, 4, 8, 8, 16}
};

static MMF_CHN_S timestamp_channel;
static int timestamp_created;
static int timestamp_attached;
static int timestamp_visible;
static int timestamp_clock_warned;
static int timestamp_polled;
static int timestamp_utc_offset;
static enum timestamp_date_format timestamp_format;
static uint32_t timestamp_last_poll_ms;
static unsigned long timestamp_last_seconds;
static pthread_t timestamp_prepare_thread;
static int timestamp_prepare_started;
static int timestamp_preparing;
static int timestamp_prepare_warned;
static int timestamp_prepare_group;
static unsigned int timestamp_prepare_width;
static unsigned int timestamp_prepare_height;

static int read_clock_seconds(unsigned long *seconds)
{
    size_t interrupt_state = csi_irq_save();
    int result = hal_cvi_rtc_get_time_sec(CVI_RTC_BASE, seconds);
    csi_irq_restore(interrupt_state);
    return result;
}

static int load_timestamp_settings(void)
{
    timestamp_utc_offset = BF01_TIMESTAMP_UTC_OFFSET_MINUTES;
    timestamp_format = (enum timestamp_date_format)BF01_TIMESTAMP_FORMAT;
    printf("[boot-timestamp] enabled=%d format=%d utc_offset_minutes=%d config=embedded source=RTC\n",
        BF01_TIMESTAMP_ENABLED, BF01_TIMESTAMP_FORMAT, BF01_TIMESTAMP_UTC_OFFSET_MINUTES);
    return BF01_TIMESTAMP_ENABLED;
}

static const uint8_t *timestamp_glyph(char character)
{
    if (character >= '0' && character <= '9')
        return timestamp_font[character - '0'];
    if (character == '-')
        return timestamp_font[10];
    if (character == ':')
        return timestamp_font[11];
    if (character == '/')
        return timestamp_font[12];
    return NULL;
}

static void draw_timestamp(RGN_CANVAS_INFO_S *canvas, const char *text)
{
    for (unsigned int row = 0; row < TIMESTAMP_HEIGHT; row++)
        memset(canvas->pu8VirtAddr + row * canvas->u32Stride, 0, canvas->u32Stride);
    for (int pass = 0; pass < 2; pass++) {
        int outline = pass == 0;
        uint16_t color = outline ? 0x8000U : 0xFFFFU;
        for (unsigned int index = 0; index < 19 && text[index]; index++) {
            const uint8_t *glyph = timestamp_glyph(text[index]);
            if (!glyph)
                continue;
            for (int glyph_row = 0; glyph_row < 7; glyph_row++) {
                for (int glyph_column = 0; glyph_column < 5; glyph_column++) {
                    if (!(glyph[glyph_row] & (1U << (4 - glyph_column))))
                        continue;
                    int left = 4 + index * TIMESTAMP_CHAR_WIDTH + glyph_column * TIMESTAMP_FONT_SCALE;
                    int top = 5 + glyph_row * TIMESTAMP_FONT_SCALE;
                    for (int row = top - outline; row < top + TIMESTAMP_FONT_SCALE + outline; row++) {
                        uint16_t *pixels = (uint16_t *)(canvas->pu8VirtAddr + row * canvas->u32Stride);
                        for (int column = left - outline;
                            column < left + TIMESTAMP_FONT_SCALE + outline; column++)
                            pixels[column] = color;
                    }
                }
            }
        }
    }
}

static CVI_S32 show_timestamp(int visible)
{
    RGN_CHN_ATTR_S attributes = {0};
    CVI_S32 result = CVI_RGN_GetDisplayAttr(TIMESTAMP_HANDLE, &timestamp_channel, &attributes);
    if (result != CVI_SUCCESS)
        return result;
    attributes.bShow = visible ? CVI_TRUE : CVI_FALSE;
    result = CVI_RGN_SetDisplayAttr(TIMESTAMP_HANDLE, &timestamp_channel, &attributes);
    if (result == CVI_SUCCESS)
        timestamp_visible = visible;
    return result;
}

static void stop_timestamp(void)
{
    if (timestamp_attached) {
        CVI_S32 result = CVI_RGN_DetachFromChn(TIMESTAMP_HANDLE, &timestamp_channel);
        if (result != CVI_SUCCESS)
            printf("[boot-timestamp] region detach failed: ret=%#x\n", (unsigned int)result);
    }
    if (timestamp_created) {
        CVI_S32 result = CVI_RGN_Destroy(TIMESTAMP_HANDLE);
        if (result != CVI_SUCCESS)
            printf("[boot-timestamp] region destroy failed: ret=%#x\n", (unsigned int)result);
    }
    timestamp_created = 0;
    timestamp_attached = 0;
    timestamp_visible = 0;
}

void APP_BootTimestampStop(void)
{
    if (timestamp_prepare_started) {
        int result = pthread_join(timestamp_prepare_thread, NULL);
        if (result) {
            printf("[boot-timestamp] prepare join failed: %d\n", result);
            return;
        }
        timestamp_prepare_started = 0;
    }
    stop_timestamp();
}

static void update_timestamp(void)
{
    if (!timestamp_attached)
        return;
    uint32_t now_ms = (uint32_t)aos_now_ms();
    if (timestamp_polled && now_ms - timestamp_last_poll_ms < TIMESTAMP_POLL_MS)
        return;
    timestamp_polled = 1;
    timestamp_last_poll_ms = now_ms;
    unsigned long seconds = 0;
    if (read_clock_seconds(&seconds) != 0 ||
        seconds < BF01_TIMESTAMP_MIN_SECONDS || seconds >= BF01_TIMESTAMP_MAX_SECONDS) {
        if (!timestamp_clock_warned) {
            printf("[boot-timestamp] RTC not synchronized; watermark hidden until valid\n");
            timestamp_clock_warned = 1;
        }
        if (timestamp_visible && show_timestamp(0) != CVI_SUCCESS) {
            printf("[boot-timestamp] cannot hide invalid timestamp; watermark stopped\n");
            stop_timestamp();
        }
        return;
    }
    timestamp_clock_warned = 0;
    if (timestamp_visible && timestamp_last_seconds == seconds)
        return;
    time_t local_seconds = (time_t)((int64_t)seconds + (int64_t)timestamp_utc_offset * 60);
    struct tm local_time;
    if (!gmtime_r(&local_seconds, &local_time))
        return;
    char text[20];
    int length;
    if (timestamp_format == TIMESTAMP_DATE_MDY) {
        length = snprintf(text, sizeof(text), "%02d/%02d/%04d %02d:%02d:%02d",
            local_time.tm_mon + 1, local_time.tm_mday, local_time.tm_year + 1900,
            local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    } else if (timestamp_format == TIMESTAMP_DATE_DMY) {
        length = snprintf(text, sizeof(text), "%02d-%02d-%04d %02d:%02d:%02d",
            local_time.tm_mday, local_time.tm_mon + 1, local_time.tm_year + 1900,
            local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    } else {
        length = snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d",
            local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
            local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    }
    if (length != 19)
        return;
    RGN_CANVAS_INFO_S canvas = {0};
    CVI_S32 result = CVI_RGN_GetCanvasInfo(TIMESTAMP_HANDLE, &canvas);
    if (result == CVI_SUCCESS && (!canvas.pu8VirtAddr ||
        canvas.enPixelFormat != PIXEL_FORMAT_ARGB_1555 ||
        canvas.stSize.u32Width < TIMESTAMP_WIDTH || canvas.stSize.u32Height < TIMESTAMP_HEIGHT ||
        canvas.u32Stride < TIMESTAMP_WIDTH * sizeof(uint16_t) || canvas.u32Stride % sizeof(uint16_t)))
        result = CVI_FAILURE;
    if (result == CVI_SUCCESS) {
        draw_timestamp(&canvas, text);
        result = CVI_RGN_UpdateCanvas(TIMESTAMP_HANDLE);
    }
    if (result == CVI_SUCCESS && !timestamp_visible)
        result = show_timestamp(1);
    if (result != CVI_SUCCESS) {
        printf("[boot-timestamp] region update failed: ret=%#x; capture continues without watermark\n",
            (unsigned int)result);
        stop_timestamp();
        return;
    }
    timestamp_last_seconds = seconds;
}

void APP_BootTimestampUpdate(void)
{
    if (__atomic_load_n(&timestamp_preparing, __ATOMIC_ACQUIRE)) {
        if (!timestamp_prepare_warned) {
            printf("[boot-timestamp] prepare pending; photo does not wait for watermark\n");
            timestamp_prepare_warned = 1;
        }
        return;
    }
    update_timestamp();
}

static void start_timestamp(int vpss_group, unsigned int width, unsigned int height)
{
    stop_timestamp();
    if (!load_timestamp_settings())
        return;
    if (width < TIMESTAMP_WIDTH + 32U || height < TIMESTAMP_HEIGHT + 32U) {
        printf("[boot-timestamp] output too small: %ux%u; watermark disabled\n", width, height);
        return;
    }
    timestamp_channel.enModId = CVI_ID_VPSS;
    timestamp_channel.s32DevId = vpss_group;
    timestamp_channel.s32ChnId = 0;
    timestamp_polled = 0;
    timestamp_clock_warned = 0;
    timestamp_last_seconds = 0;
    RGN_ATTR_S region = {0};
    region.enType = OVERLAY_RGN;
    region.unAttr.stOverlay.enPixelFormat = PIXEL_FORMAT_ARGB_1555;
    region.unAttr.stOverlay.stSize.u32Width = TIMESTAMP_WIDTH;
    region.unAttr.stOverlay.stSize.u32Height = TIMESTAMP_HEIGHT;
    region.unAttr.stOverlay.u32CanvasNum = 2;
    CVI_S32 result = CVI_RGN_Create(TIMESTAMP_HANDLE, &region);
    if (result == CVI_SUCCESS) {
        timestamp_created = 1;
        RGN_CHN_ATTR_S attributes = {0};
        attributes.enType = OVERLAY_RGN;
        attributes.bShow = CVI_FALSE;
        attributes.unChnAttr.stOverlayChn.stPoint.s32X = 32;
        attributes.unChnAttr.stOverlayChn.stPoint.s32Y = height - 64U;
        attributes.unChnAttr.stOverlayChn.u32Layer = 0;
        result = CVI_RGN_AttachToChn(TIMESTAMP_HANDLE, &timestamp_channel, &attributes);
    }
    if (result != CVI_SUCCESS) {
        printf("[boot-timestamp] region start failed: ret=%#x; capture continues without watermark\n",
            (unsigned int)result);
        stop_timestamp();
        return;
    }
    timestamp_attached = 1;
    update_timestamp();
}

void APP_BootTimestampStart(int vpss_group, unsigned int width, unsigned int height)
{
    APP_BootTimestampStop();
    if (!timestamp_prepare_started)
        start_timestamp(vpss_group, width, height);
}

static void *prepare_timestamp(void *argument)
{
    (void)argument;
    uint64_t started_us = csi_tick_get_us();
    start_timestamp(timestamp_prepare_group, timestamp_prepare_width, timestamp_prepare_height);
    printf("[boot-timestamp] prepare done: group=%d attached=%d elapsed_us=%llu boot_ms=%llu\n",
        timestamp_prepare_group, timestamp_attached,
        (unsigned long long)(csi_tick_get_us() - started_us),
        (unsigned long long)(csi_tick_get_us() / 1000));
    __atomic_store_n(&timestamp_preparing, 0, __ATOMIC_RELEASE);
    return NULL;
}

void APP_BootTimestampPrepare(int vpss_group, unsigned int width, unsigned int height)
{
    if (!BF01_TIMESTAMP_ENABLED)
        return;
    APP_BootTimestampStop();
    if (timestamp_prepare_started)
        return;
    timestamp_prepare_group = vpss_group;
    timestamp_prepare_width = width;
    timestamp_prepare_height = height;
    timestamp_prepare_warned = 0;
    __atomic_store_n(&timestamp_preparing, 1, __ATOMIC_RELEASE);
    pthread_attr_t attributes;
    int result = pthread_attr_init(&attributes);
    if (!result) {
        result = pthread_attr_setstacksize(&attributes, 16U * 1024);
        struct sched_param schedule = {0};
        int policy = 0;
        if (!result)
            result = pthread_getschedparam(pthread_self(), &policy, &schedule);
        if (!result) {
            int minimum = sched_get_priority_min(policy);
            if (minimum < 0 || schedule.sched_priority <= minimum) {
                result = ENOTSUP;
            } else {
                schedule.sched_priority--;
                result = pthread_attr_setschedparam(&attributes, &schedule);
            }
        }
        if (!result)
            result = pthread_create(&timestamp_prepare_thread, &attributes, prepare_timestamp, NULL);
        pthread_attr_destroy(&attributes);
    }
    if (result) {
        __atomic_store_n(&timestamp_preparing, 0, __ATOMIC_RELEASE);
        printf("[boot-timestamp] prepare thread failed: %d; capture continues without watermark\n", result);
    } else {
        timestamp_prepare_started = 1;
    }
}

static int synchronize_clock(void *private_data, IPCM_ANON_MSG_S *message)
{
    (void)private_data;
    if (!message || message->u8PortID != BF01_TIMESTAMP_CLOCK_PORT ||
        message->u8DataType != IPCM_MSG_TYPE_RAW_PARAM ||
        (message->u8MsgID != BF01_TIMESTAMP_CLOCK_SET &&
         message->u8MsgID != BF01_TIMESTAMP_CLOCK_SET_ACK))
        return -EINVAL;
    uint32_t seconds = message->u32Param;
    uint32_t confirmed = 0;
    if (seconds >= BF01_TIMESTAMP_MIN_SECONDS && seconds < BF01_TIMESTAMP_MAX_SECONDS) {
        size_t interrupt_state = csi_irq_save();
        hal_cvi_rtc_clk_set(1);
        mmio_clrbits_32(CVI_RTC_BASE + CVI_RTC_SEC_PULSE_GEN, 1U << 31);
        mmio_clrbits_32(CVI_RTC_BASE + CVI_RTC_ANA_CALIB, 1U << 31);
        hal_cvi_rtc_set_time(CVI_RTC_BASE, seconds);
        csi_irq_restore(interrupt_state);
        for (unsigned int attempt = 0; attempt < 10; attempt++) {
            unsigned long observed = 0;
            if (read_clock_seconds(&observed) == 0 &&
                observed >= seconds && observed - seconds <= 1) {
                confirmed = (uint32_t)observed;
                break;
            }
            aos_msleep(1);
        }
    }
    printf("[boot-timestamp] RTC sync: requested=%u confirmed=%u error=%d\n",
        seconds, confirmed, confirmed ? 0 : EIO);
    if (message->u8MsgID == BF01_TIMESTAMP_CLOCK_SET_ACK)
        return CVI_IPCM_AnonSendParam(BF01_TIMESTAMP_CLOCK_REPLY_PORT,
            BF01_TIMESTAMP_CLOCK_SET_ACK, confirmed);
    return confirmed ? 0 : -EIO;
}

void APP_BootTimestampInitClockSync(void)
{
    int result = CVI_IPCM_RegisterAnonHandle(BF01_TIMESTAMP_CLOCK_PORT, synchronize_clock, NULL);
    if (result != CVI_SUCCESS)
        printf("[boot-timestamp] RTC sync registration failed: %d; capture continues\n", result);
}
