#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <aos/kernel.h>
#include "bf01_boot_ai.h"
#include "bf01_boot_capture.h"
#include "bf01_boot_video.h"
#include "cvi_board_memmap.h"
#include "cvi_ipcm.h"
#include "cvi_sys.h"
#include "cvi_vpss.h"

static pthread_mutex_t ai_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char *ai_image;
static uint32_t ai_started_ms;
static struct bf01_ai_reply ai_status = {
    .magic = BF01_AI_MAGIC,
    .version = BF01_AI_VERSION,
    .state = BF01_AI_WAITING,
    .width = BF01_AI_WIDTH,
    .height = BF01_AI_HEIGHT,
    .ion_base = CVIMMAP_ION_ADDR,
    .ion_size = CVIMMAP_ION_SIZE,
    .rtos_ion_size = CVIMMAP_FREERTOS_RESERVED_ION_SIZE,
    .rtos_base = CVIMMAP_FREERTOS_ADDR,
    .rtos_size = CVIMMAP_FREERTOS_SIZE,
};

int APP_BootAiRejected(void)
{
    pthread_mutex_lock(&ai_mutex);
    int rejected = ai_status.state == BF01_AI_REJECTED || ai_status.state == BF01_AI_FAILED;
    pthread_mutex_unlock(&ai_mutex);
    return rejected;
}

void APP_BootAiFail(int error)
{
    int rejected = 0;
    pthread_mutex_lock(&ai_mutex);
    if (ai_status.state == BF01_AI_WAITING || ai_status.state == BF01_AI_READY) {
        ai_status.state = BF01_AI_FAILED;
        ai_status.error = error ? error : EIO;
        rejected = 1;
    }
    pthread_mutex_unlock(&ai_mutex);
    if (rejected) {
        APP_BootVideoReject();
        APP_BootCaptureReject();
    }
}

static int ai_message(void *private_data, IPCM_ANON_MSG_S *message)
{
    (void)private_data;
    if (!message || message->u8PortID != BF01_AI_PORT ||
        message->u8MsgID != BF01_AI_VERSION || message->u8DataType != IPCM_MSG_TYPE_SHM ||
        message->stData.u32Size != sizeof(struct bf01_ai_request) || !message->stData.pData)
        return -EINVAL;
    struct bf01_ai_request request;
    memcpy(&request, message->stData.pData, sizeof(request));
    if (request.magic != BF01_AI_MAGIC || request.version != BF01_AI_VERSION)
        return -EPROTO;
    struct bf01_ai_reply *reply = CVI_IPCM_GetBuff(sizeof(*reply));
    if (!reply)
        return -ENOMEM;
    int reject = 0;
    pthread_mutex_lock(&ai_mutex);
    int error = 0;
    if (request.command == BF01_AI_ACCEPT) {
        if (request.image_paddr != ai_status.image_paddr || !request.image_paddr ||
            (ai_status.state != BF01_AI_PASSED &&
                (uint32_t)aos_now_ms() - ai_started_ms >= BF01_AI_TIMEOUT_MS) ||
            (ai_status.state != BF01_AI_READY && ai_status.state != BF01_AI_PASSED)) {
            error = EPERM;
        } else {
            ai_status.state = BF01_AI_PASSED;
        }
    } else if (request.command == BF01_AI_REJECT) {
        ai_status.state = BF01_AI_REJECTED;
        reject = 1;
    } else if (request.command != BF01_AI_STATUS) {
        error = EINVAL;
    }
    if (!error && request.command != BF01_AI_STATUS) {
        aos_ion_free(ai_image);
        ai_image = NULL;
    }
    *reply = ai_status;
    reply->sequence = request.sequence;
    if (error)
        reply->error = error;
    pthread_mutex_unlock(&ai_mutex);
    if (reject) {
        APP_BootVideoReject();
        APP_BootCaptureReject();
    }
    int result = CVI_IPCM_AnonSendMsg(BF01_AI_PORT, BF01_AI_VERSION, reply, sizeof(*reply));
    if (result < 0)
        CVI_IPCM_ReleaseBuff(reply);
    return result;
}

static void *ai_watchdog(void *argument)
{
    (void)argument;
    for (;;) {
        pthread_mutex_lock(&ai_mutex);
        int pending = ai_status.state == BF01_AI_WAITING || ai_status.state == BF01_AI_READY;
        pthread_mutex_unlock(&ai_mutex);
        if (!pending)
            return NULL;
        if ((uint32_t)aos_now_ms() - ai_started_ms >= BF01_AI_TIMEOUT_MS) {
            printf("[boot-ai] decision timeout; stop RAM recording and discard photos\n");
            APP_BootAiFail(ETIMEDOUT);
            return NULL;
        }
        aos_msleep(100);
    }
}

int APP_BootAiInit(void)
{
    ai_started_ms = (uint32_t)aos_now_ms();
    int result = CVI_IPCM_RegisterAnonHandle(BF01_AI_PORT, ai_message, NULL);
    if (result)
        return result;
    pthread_t thread;
    pthread_attr_t attributes;
    result = pthread_attr_init(&attributes);
    if (!result) {
        result = pthread_attr_setstacksize(&attributes, 16U * 1024);
        if (!result)
            result = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        if (!result)
            result = pthread_create(&thread, &attributes, ai_watchdog, NULL);
        pthread_attr_destroy(&attributes);
    }
    return result;
}

int APP_BootAiCapture(int publish, uint64_t expected_pts)
{
    VIDEO_FRAME_INFO_S frame = {0};
    int error = 0;
    unsigned char *image = NULL;
    if (!publish) {
        while (CVI_VPSS_GetChnFrame(1, 1, &frame, 0) == CVI_SUCCESS)
            if (CVI_VPSS_ReleaseChnFrame(1, 1, &frame) != CVI_SUCCESS)
                return EIO;
        return 0;
    }
    if (CVI_VPSS_GetChnFrame(1, 1, &frame, 1000) != CVI_SUCCESS)
        return EIO;
    if (frame.stVFrame.u32Width != BF01_AI_WIDTH || frame.stVFrame.u32Height != BF01_AI_HEIGHT ||
        frame.stVFrame.enPixelFormat != PIXEL_FORMAT_RGB_888_PLANAR ||
        frame.stVFrame.u64PTS != expected_pts)
        error = EPROTO;
    if (!error && publish && !APP_BootAiRejected()) {
        image = aos_ion_malloc(BF01_AI_IMAGE_SIZE);
        if (!image)
            error = ENOMEM;
        for (unsigned int plane = 0; !error && plane < 3; plane++) {
            uint32_t stride = frame.stVFrame.u32Stride[plane];
            uint32_t length = frame.stVFrame.u32Length[plane];
            if (stride < BF01_AI_WIDTH || stride > UINT32_MAX / BF01_AI_HEIGHT ||
                length < stride * BF01_AI_HEIGHT || !frame.stVFrame.u64PhyAddr[plane]) {
                error = EPROTO;
                break;
            }
            const unsigned char *source = (const unsigned char *)(uintptr_t)frame.stVFrame.u64PhyAddr[plane];
            if (CVI_SYS_IonInvalidateCache(frame.stVFrame.u64PhyAddr[plane], (void *)source, length) != CVI_SUCCESS)
                error = EIO;
            for (unsigned int row = 0; !error && row < BF01_AI_HEIGHT; row++)
                memcpy(image + (plane * BF01_AI_HEIGHT + row) * BF01_AI_WIDTH,
                    source + row * stride, BF01_AI_WIDTH);
        }
        if (!error && CVI_SYS_IonFlushCache((uintptr_t)image, image, BF01_AI_IMAGE_SIZE) != CVI_SUCCESS)
            error = EIO;
    }
    if (CVI_VPSS_ReleaseChnFrame(1, 1, &frame) != CVI_SUCCESS)
        error = EIO;
    if (publish && CVI_VPSS_DisableChn(1, 1) != CVI_SUCCESS)
        error = EIO;
    pthread_mutex_lock(&ai_mutex);
    if (!error && image && ai_status.state == BF01_AI_WAITING) {
        ai_image = image;
        image = NULL;
        ai_status.image_paddr = (uint32_t)(uintptr_t)ai_image;
        ai_status.image_size = BF01_AI_IMAGE_SIZE;
        ai_status.pts_low = (uint32_t)frame.stVFrame.u64PTS;
        ai_status.pts_high = (uint32_t)(frame.stVFrame.u64PTS >> 32);
        ai_status.state = BF01_AI_READY;
        printf("[boot-ai] ready: VPSS(1,1) RGB planar 640x640 bytes=%u paddr=%#x pts=%llu\n",
            ai_status.image_size, ai_status.image_paddr, (unsigned long long)frame.stVFrame.u64PTS);
    }
    pthread_mutex_unlock(&ai_mutex);
    aos_ion_free(image);
    return error;
}
