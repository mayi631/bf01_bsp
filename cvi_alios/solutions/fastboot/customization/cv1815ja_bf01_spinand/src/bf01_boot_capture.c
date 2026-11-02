#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <aos/kernel.h>
#include <drv/tick.h>
#include "bf01_boot_capture.h"
#include "bf01_boot_video.h"
#include "cvi_ipcm.h"
#include "cvi_sys.h"
#include "cvi_venc.h"
#include "cvi_vi.h"
#include "cvi_vpss.h"

#define BURST_JPEG_CHANNEL 3
#define BURST_VI_PIPE 0
#define BURST_VI_CHANNEL 0
#define BURST_VPSS_GROUP 1
#define BURST_PACKS 8
#define BURST_STACK_SIZE (32U * 1024)
#define BURST_TIMEOUT_MS 2000

static pthread_mutex_t burst_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct bf01_burst_reply burst = {
    .magic = BF01_BURST_MAGIC,
    .version = BF01_BURST_VERSION,
    .width = BF01_BURST_WIDTH,
    .height = BF01_BURST_HEIGHT
};
static unsigned char *burst_images[BF01_BURST_COUNT];

static int burst_message(void *private_data, IPCM_ANON_MSG_S *message)
{
    (void)private_data;
    if (!message || message->u8PortID != BF01_BURST_PORT ||
        message->u8MsgID != BF01_BURST_VERSION || message->u8DataType != IPCM_MSG_TYPE_SHM ||
        message->stData.u32Size != sizeof(struct bf01_burst_request) || !message->stData.pData)
        return -EINVAL;
    struct bf01_burst_request request;
    memcpy(&request, message->stData.pData, sizeof(request));
    if (request.magic != BF01_BURST_MAGIC || request.version != BF01_BURST_VERSION)
        return -EPROTO;
    struct bf01_burst_reply *reply = CVI_IPCM_GetBuff(sizeof(*reply));
    if (!reply)
        return -ENOMEM;
    pthread_mutex_lock(&burst_mutex);
    *reply = burst;
    reply->sequence = request.sequence;
    reply->command = request.command;
    reply->index = request.index;
    reply->offset = request.offset;
    reply->length = 0;
    if (request.command == BF01_BURST_READ) {
        if (burst.state != BF01_BURST_READY || request.index >= burst.count ||
            request.offset >= burst.images[request.index].size) {
            reply->error = EINVAL;
        } else {
            uint32_t remaining = burst.images[request.index].size - request.offset;
            reply->length = remaining < BF01_BURST_CHUNK ? remaining : BF01_BURST_CHUNK;
            memcpy(reply->data, burst_images[request.index] + request.offset, reply->length);
        }
    } else if (request.command == BF01_BURST_COMMIT) {
        uint32_t checksum = 0;
        for (unsigned int index = 0; index < burst.count; index++)
            checksum ^= burst.images[index].crc;
        if ((burst.state != BF01_BURST_READY && burst.state != BF01_BURST_RELEASED) ||
            request.offset != checksum) {
            reply->error = EINVAL;
        } else {
            for (unsigned int index = 0; index < BF01_BURST_COUNT; index++) {
                aos_ion_free(burst_images[index]);
                burst_images[index] = NULL;
            }
            burst.state = BF01_BURST_RELEASED;
            reply->state = burst.state;
        }
    } else if (request.command != BF01_BURST_STATUS) {
        reply->error = EINVAL;
    }
    pthread_mutex_unlock(&burst_mutex);
    int result = CVI_IPCM_AnonSendMsg(BF01_BURST_PORT, BF01_BURST_VERSION, reply, sizeof(*reply));
    if (result < 0)
        CVI_IPCM_ReleaseBuff(reply);
    return result;
}

static int prime_jpeg_input(void)
{
    VIDEO_FRAME_INFO_S frame = {0};
    uint32_t started_ms = (uint32_t)aos_now_ms();
    printf("[boot-capture] prime begin: started_ms=%u VI(%d,%d) -> VPSS(%d); no JPEG expected\n",
        started_ms, BURST_VI_PIPE, BURST_VI_CHANNEL, BURST_VPSS_GROUP);
    CVI_S32 result = CVI_VI_GetChnFrame(BURST_VI_PIPE, BURST_VI_CHANNEL, &frame, BURST_TIMEOUT_MS);
    if (result != CVI_SUCCESS) {
        printf("[boot-capture] prime VI GetChnFrame failed: ret=%#x elapsed_ms=%u\n",
            (unsigned int)result, (uint32_t)aos_now_ms() - started_ms);
        return EIO;
    }
    uint32_t captured_ms = (uint32_t)aos_now_ms();
    int error = 0;
    if (!frame.stVFrame.u32Width || !frame.stVFrame.u32Height) {
        error = EINVAL;
    } else {
        result = CVI_VPSS_SendFrame(BURST_VPSS_GROUP, &frame, BURST_TIMEOUT_MS);
        if (result != CVI_SUCCESS)
            error = EIO;
        printf("[boot-capture] prime VPSS SendFrame: ret=%#x captured_ms=%u pts=%llu\n",
            (unsigned int)result, captured_ms, (unsigned long long)frame.stVFrame.u64PTS);
    }
    result = CVI_VI_ReleaseChnFrame(BURST_VI_PIPE, BURST_VI_CHANNEL, &frame);
    if (result != CVI_SUCCESS) {
        printf("[boot-capture] prime VI ReleaseChnFrame failed: ret=%#x\n", (unsigned int)result);
        error = EIO;
    }
    uint32_t completed_ms = (uint32_t)aos_now_ms();
    printf("[boot-capture] prime submitted: error=%d completed_ms=%u elapsed_ms=%u; excluded from burst\n",
        error, completed_ms, completed_ms - started_ms);
    return error;
}

static int capture_image(unsigned int index)
{
    VIDEO_FRAME_INFO_S frame = {0};
    VENC_CHN_STATUS_S status = {0};
    VENC_PACK_S *packs = NULL;
    VENC_STREAM_S stream = {0};
    int frame_held = 0;
    int stream_held = 0;
    unsigned char *image = NULL;
    int error = EIO;
    size_t size = 0;
    uint32_t crc = UINT32_MAX;
    uint32_t captured_ms = 0;
    uint64_t captured_boot_us = 0;
    uint64_t pts = 0;
    CVI_S32 result;
    result = CVI_VI_GetChnFrame(BURST_VI_PIPE, BURST_VI_CHANNEL, &frame, BURST_TIMEOUT_MS);
    if (result != CVI_SUCCESS) {
        printf("[boot-capture] VI GetChnFrame failed: pipe=%d channel=%d ret=%#x\n",
            BURST_VI_PIPE, BURST_VI_CHANNEL, (unsigned int)result);
        goto done;
    }
    frame_held = 1;
    captured_boot_us = csi_tick_get_us();
    captured_ms = (uint32_t)aos_now_ms();
    pts = frame.stVFrame.u64PTS;
    if (!frame.stVFrame.u32Width || !frame.stVFrame.u32Height) {
        printf("[boot-capture] invalid VI frame size: %ux%u\n",
            frame.stVFrame.u32Width, frame.stVFrame.u32Height);
        error = EINVAL;
        goto done;
    }
    printf("[boot-capture] VI frame ready: %ux%u pts=%llu captured_ms=%u\n",
        frame.stVFrame.u32Width, frame.stVFrame.u32Height, (unsigned long long)pts, captured_ms);
    int64_t operation_started_ms = aos_now_ms();
    printf("[boot-capture] VPSS SendFrame begin: photo=%u group=%d timeout_ms=%d\n",
        index + 1, BURST_VPSS_GROUP, BURST_TIMEOUT_MS);
    result = CVI_VPSS_SendFrame(BURST_VPSS_GROUP, &frame, BURST_TIMEOUT_MS);
    printf("[boot-capture] VPSS SendFrame end: photo=%u ret=%#x elapsed_ms=%llu\n",
        index + 1, (unsigned int)result, (unsigned long long)(aos_now_ms() - operation_started_ms));
    if (result != CVI_SUCCESS) {
        printf("[boot-capture] VPSS SendFrame failed: group=%d ret=%#x\n",
            BURST_VPSS_GROUP, (unsigned int)result);
        goto done;
    }
    operation_started_ms = aos_now_ms();
    do {
        result = CVI_VENC_QueryStatus(BURST_JPEG_CHANNEL, &status);
        if (result != CVI_SUCCESS) {
            printf("[boot-capture] QueryStatus failed: photo=%u ret=%#x\n",
                index + 1, (unsigned int)result);
            goto done;
        }
        if (status.u32CurPacks)
            break;
        if (aos_now_ms() - operation_started_ms >= BURST_TIMEOUT_MS) {
            printf("[boot-capture] QueryStatus timed out: photo=%u no JPEG packs\n", index + 1);
            error = ETIMEDOUT;
            goto done;
        }
        aos_msleep(1);
    } while (1);
    printf("[boot-capture] QueryStatus: photo=%u packs=%u left_frames=%u elapsed_ms=%llu\n",
        index + 1, status.u32CurPacks, status.u32LeftStreamFrames,
        (unsigned long long)(aos_now_ms() - operation_started_ms));
    if (status.u32CurPacks > BURST_PACKS) {
        error = EOVERFLOW;
        goto done;
    }
    unsigned int pack_capacity = status.u32CurPacks;
    packs = calloc(pack_capacity, sizeof(*packs));
    if (!packs) {
        error = ENOMEM;
        goto done;
    }
    unsigned int attempts = 0;
    printf("[boot-capture] GetStream begin: photo=%u channel=%d packs=%u timeout_ms=%d\n",
        index + 1, BURST_JPEG_CHANNEL, pack_capacity, BURST_TIMEOUT_MS);
    do {
        int64_t elapsed_ms = aos_now_ms() - operation_started_ms;
        if (elapsed_ms >= BURST_TIMEOUT_MS) {
            printf("[boot-capture] GetStream timed out: photo=%u attempts=%u elapsed_ms=%llu\n",
                index + 1, attempts, (unsigned long long)elapsed_ms);
            error = ETIMEDOUT;
            goto done;
        }
        stream.pstPack = packs;
        stream.u32PackCount = pack_capacity;
        attempts++;
        result = CVI_VENC_GetStream(BURST_JPEG_CHANNEL, &stream,
            BURST_TIMEOUT_MS - (CVI_S32)elapsed_ms);
        if (result != CVI_ERR_VENC_BUSY)
            break;
        printf("[boot-capture] GetStream busy: photo=%u attempt=%u elapsed_ms=%llu\n",
            index + 1, attempts, (unsigned long long)(aos_now_ms() - operation_started_ms));
        if (aos_now_ms() - operation_started_ms < BURST_TIMEOUT_MS)
            aos_msleep(1);
    } while (1);
    printf("[boot-capture] GetStream end: photo=%u ret=%#x packs=%u attempts=%u elapsed_ms=%llu\n",
        index + 1, (unsigned int)result, stream.u32PackCount, attempts,
        (unsigned long long)(aos_now_ms() - operation_started_ms));
    if (result != CVI_SUCCESS) {
        printf("[boot-capture] GetStream failed: channel=%d ret=%#x\n",
            BURST_JPEG_CHANNEL, (unsigned int)result);
        goto done;
    }
    stream_held = 1;
    if (!stream.pstPack || !stream.u32PackCount || stream.u32PackCount > pack_capacity) {
        printf("[boot-capture] invalid GetStream metadata: packs=%u\n", stream.u32PackCount);
        error = EPROTO;
        goto done;
    }
    for (unsigned int pack_index = 0; pack_index < stream.u32PackCount; pack_index++) {
        const VENC_PACK_S *pack = &stream.pstPack[pack_index];
        printf("[boot-capture] stream pack: photo=%u pack=%u length=%u offset=%u pts=%llu\n",
            index + 1, pack_index, pack->u32Len, pack->u32Offset, (unsigned long long)pack->u64PTS);
        if (!pack->pu8Addr || pack->u32Offset > pack->u32Len ||
            pack->u32Len - pack->u32Offset > BF01_BURST_MAX_JPEG - size) {
            error = EOVERFLOW;
            goto done;
        }
        size += pack->u32Len - pack->u32Offset;
    }
    if (size < 4)
        goto done;
    image = aos_ion_malloc(size);
    if (!image) {
        error = ENOMEM;
        goto done;
    }
    size_t offset = 0;
    for (unsigned int pack_index = 0; pack_index < stream.u32PackCount; pack_index++) {
        const VENC_PACK_S *pack = &stream.pstPack[pack_index];
        size_t length = pack->u32Len - pack->u32Offset;
        memcpy(image + offset, pack->pu8Addr + pack->u32Offset, length);
        crc = bf01_burst_crc(crc, image + offset, length);
        offset += length;
    }
    if (image[0] != 0xff || image[1] != 0xd8 || image[size - 2] != 0xff || image[size - 1] != 0xd9) {
        error = EBADMSG;
        goto done;
    }
    error = 0;
done:
    if (stream_held) {
        result = CVI_VENC_ReleaseStream(BURST_JPEG_CHANNEL, &stream);
        printf("[boot-capture] ReleaseStream: photo=%u ret=%#x\n", index + 1, (unsigned int)result);
        if (result != CVI_SUCCESS) {
            printf("[boot-capture] ReleaseStream failed: ret=%#x\n", (unsigned int)result);
            error = EIO;
        }
    }
    free(packs);
    if (frame_held) {
        result = CVI_VI_ReleaseChnFrame(BURST_VI_PIPE, BURST_VI_CHANNEL, &frame);
        if (result != CVI_SUCCESS) {
            printf("[boot-capture] VI ReleaseChnFrame failed: ret=%#x\n", (unsigned int)result);
            error = EIO;
        }
    }
    if (error) {
        aos_ion_free(image);
        return error;
    }
    pthread_mutex_lock(&burst_mutex);
    burst_images[index] = image;
    burst.images[index].size = (uint32_t)size;
    burst.images[index].crc = crc ^ UINT32_MAX;
    burst.images[index].pts_low = (uint32_t)pts;
    burst.images[index].pts_high = (uint32_t)(pts >> 32);
    burst.images[index].captured_ms = captured_ms;
    burst.count = index + 1;
    pthread_mutex_unlock(&burst_mutex);
    if (index == 0) {
        uint64_t completed_boot_us = csi_tick_get_us();
        printf("[boot-capture] first-photo timing: boot_to_frame_ms=%llu boot_to_photo_ms=%llu frame_to_photo_ms=%llu clock=csi_hw_counter end=jpeg_in_ram\n",
            (unsigned long long)(captured_boot_us / 1000),
            (unsigned long long)(completed_boot_us / 1000),
            (unsigned long long)((completed_boot_us - captured_boot_us) / 1000));
    }
    uint32_t gap_ms = index ? captured_ms - burst.images[index - 1].captured_ms : 0;
    uint64_t previous_pts = index ? ((uint64_t)burst.images[index - 1].pts_high << 32) |
        burst.images[index - 1].pts_low : pts;
    uint32_t completed_ms = (uint32_t)aos_now_ms();
    printf("[boot-capture] photo=%u bytes=%u captured_ms=%u pts=%llu gap_ms=%u pts_gap_us=%lld completed_ms=%u encode_copy_ms=%u\n",
        index + 1, (unsigned int)size, captured_ms, (unsigned long long)pts, gap_ms,
        (long long)pts - (long long)previous_pts, completed_ms, completed_ms - captured_ms);
    return 0;
}

static void *capture_burst(void *argument)
{
    (void)argument;
    uint32_t started_ms = (uint32_t)aos_now_ms();
    printf("[boot-capture] burst: started_ms=%u count=%u interval_ms=%u VI(%d,%d) -> VPSS_SendFrame(%d) -> VENC_GetStream(%d)\n",
        started_ms, BF01_BURST_COUNT, BF01_BURST_INTERVAL_MS, BURST_VI_PIPE, BURST_VI_CHANNEL,
        BURST_VPSS_GROUP, BURST_JPEG_CHANNEL);
    VENC_CHN_ATTR_S attributes = {0};
    CVI_S32 result = CVI_VENC_GetChnAttr(BURST_JPEG_CHANNEL, &attributes);
    int error = 0;
    if (result != CVI_SUCCESS) {
        printf("[boot-capture] GetChnAttr failed: channel=%d ret=%#x\n",
            BURST_JPEG_CHANNEL, (unsigned int)result);
        error = EIO;
    } else if (attributes.stVencAttr.enType != PT_JPEG ||
        attributes.stVencAttr.u32PicWidth != BF01_BURST_WIDTH ||
        attributes.stVencAttr.u32PicHeight != BF01_BURST_HEIGHT) {
        printf("[boot-capture] unexpected JPEG attributes: codec=%u size=%ux%u\n",
            (unsigned int)attributes.stVencAttr.enType, attributes.stVencAttr.u32PicWidth,
            attributes.stVencAttr.u32PicHeight);
        error = EINVAL;
    }
    if (!error) {
        printf("[boot-capture] sample-style JPEG: size=%ux%u es_queue=%u; QueryStatus -> GetStream -> copy -> ReleaseStream\n",
            attributes.stVencAttr.u32PicWidth, attributes.stVencAttr.u32PicHeight,
            (unsigned int)attributes.stVencAttr.bEsBufQueueEn);
        error = prime_jpeg_input();
    }
    for (unsigned int index = 0; !error && index < BF01_BURST_COUNT; index++) {
        if (index) {
            uint32_t elapsed_ms = (uint32_t)aos_now_ms() - burst.images[index - 1].captured_ms;
            if (elapsed_ms < BF01_BURST_INTERVAL_MS)
                aos_msleep(BF01_BURST_INTERVAL_MS - elapsed_ms);
        }
        error = capture_image(index);
        if (error) {
            printf("[boot-capture] photo=%u failed: error=%d\n", index + 1, error);
            break;
        }
    }
    pthread_mutex_lock(&burst_mutex);
    burst.error = error;
    burst.state = error ? BF01_BURST_FAILED : BF01_BURST_READY;
    pthread_mutex_unlock(&burst_mutex);
    uint32_t completed_ms = (uint32_t)aos_now_ms();
    printf("[boot-capture] burst finished: count=%u error=%d completed_ms=%u elapsed_ms=%u; retained until SD commit\n",
        burst.count, error, completed_ms, completed_ms - started_ms);
    if (!error)
        APP_BootVideoStart();
    return NULL;
}

void APP_BootCaptureStart(int media_result)
{
    int video_result = APP_BootVideoInit();
    if (video_result != CVI_SUCCESS)
        printf("[boot-video] IPC registration failed: %d\n", video_result);
    uint32_t media_ready_ms = (uint32_t)aos_now_ms();
    pthread_mutex_lock(&burst_mutex);
    burst.state = media_result == CVI_SUCCESS ? BF01_BURST_CAPTURING : BF01_BURST_FAILED;
    burst.error = media_result == CVI_SUCCESS ? 0 : EIO;
    pthread_mutex_unlock(&burst_mutex);
    int result = CVI_IPCM_RegisterAnonHandle(BF01_BURST_PORT, burst_message, NULL);
    if (result != CVI_SUCCESS) {
        printf("[boot-capture] IPC registration failed: %d; capture continues in RAM\n", result);
    }
    if (media_result != CVI_SUCCESS) {
        printf("[boot-capture] media init failed: ret=%#x\n", (unsigned int)media_result);
        return;
    }
    pthread_t thread;
    pthread_attr_t attributes;
    result = pthread_attr_init(&attributes);
    if (!result) {
        result = pthread_attr_setstacksize(&attributes, BURST_STACK_SIZE);
        if (!result)
            result = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        if (!result)
            result = pthread_create(&thread, &attributes, capture_burst, NULL);
        pthread_attr_destroy(&attributes);
    }
    if (result) {
        pthread_mutex_lock(&burst_mutex);
        burst.state = BF01_BURST_FAILED;
        burst.error = result > 0 ? result : EAGAIN;
        pthread_mutex_unlock(&burst_mutex);
        printf("[boot-capture] thread creation failed: %d\n", result);
    } else {
        printf("[boot-capture] worker started: media_ready_ms=%u stack_bytes=%u\n",
            media_ready_ms, BURST_STACK_SIZE);
    }
}
