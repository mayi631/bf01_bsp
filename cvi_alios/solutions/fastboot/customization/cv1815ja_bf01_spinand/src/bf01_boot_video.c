#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <aos/kernel.h>
#include <drv/tick.h>
#include "bf01_boot_video.h"
#include "bf01_boot_audio.h"
#include "cvi_ipcm.h"
#include "cvi_sys.h"
#include "cvi_venc.h"
#include "cvi_vi.h"
#include "cvi_vpss.h"

#define VIDEO_CHANNEL 4
#define VIDEO_PACK_CAPACITY 32
#define VIDEO_TIMEOUT_MS 1000
#define VIDEO_MAX_SECONDS 330

static pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct bf01_video_reply video_status = {
    .magic = BF01_VIDEO_MAGIC,
    .version = BF01_VIDEO_VERSION,
    .state = BF01_VIDEO_STARTING
};
static struct bf01_video_ring *video_ring;
static CVI_U64 video_physical;
static int video_stop;
static int video_started;
static uint32_t video_offset;

static void flush_region(void *address, uint32_t size)
{
    CVI_SYS_IonFlushCache(video_physical + ((unsigned char *)address - (unsigned char *)video_ring),
        address, size);
}

static void set_state(uint32_t state, int error)
{
    pthread_mutex_lock(&video_mutex);
    video_status.state = state;
    video_status.error = error;
    if (video_ring) {
        video_ring->state = state;
        flush_region(video_ring, offsetof(struct bf01_video_ring, frames));
    }
    pthread_mutex_unlock(&video_mutex);
}

static int stopping(void)
{
    pthread_mutex_lock(&video_mutex);
    int stop = video_stop;
    pthread_mutex_unlock(&video_mutex);
    return stop;
}

static int video_message(void *private_data, IPCM_ANON_MSG_S *message)
{
    (void)private_data;
    if (!message || message->u8PortID != BF01_VIDEO_PORT ||
        message->u8MsgID != BF01_VIDEO_VERSION || message->u8DataType != IPCM_MSG_TYPE_SHM ||
        message->stData.u32Size != sizeof(struct bf01_video_request) || !message->stData.pData)
        return -EINVAL;
    struct bf01_video_request request;
    memcpy(&request, message->stData.pData, sizeof(request));
    if (request.magic != BF01_VIDEO_MAGIC || request.version != BF01_VIDEO_VERSION)
        return -EPROTO;
    struct bf01_video_reply *reply = CVI_IPCM_GetBuff(sizeof(*reply));
    if (!reply)
        return -ENOMEM;
    pthread_mutex_lock(&video_mutex);
    *reply = video_status;
    reply->sequence = request.sequence;
    if (request.command == BF01_VIDEO_STOP) {
        video_stop = 1;
    } else if (request.command == BF01_VIDEO_RELEASE) {
        if (video_status.state != BF01_VIDEO_READY && video_status.state != BF01_VIDEO_FAILED &&
            video_status.state != BF01_VIDEO_RELEASED) {
            reply->error = EBUSY;
        } else {
            if (video_ring)
                CVI_SYS_IonFree(video_physical, video_ring);
            video_ring = NULL;
            video_status.ring_paddr = 0;
            video_status.ring_size = 0;
            video_status.state = BF01_VIDEO_RELEASED;
            *reply = video_status;
            reply->sequence = request.sequence;
        }
    } else if (request.command != BF01_VIDEO_STATUS) {
        reply->error = EINVAL;
    }
    pthread_mutex_unlock(&video_mutex);
    int result = CVI_IPCM_AnonSendMsg(BF01_VIDEO_PORT, BF01_VIDEO_VERSION, reply, sizeof(*reply));
    if (result < 0)
        CVI_IPCM_ReleaseBuff(reply);
    return result;
}

int APP_BootVideoInit(void)
{
    return CVI_IPCM_RegisterAnonHandle(BF01_VIDEO_PORT, video_message, NULL);
}

static int save_stream(const VENC_STREAM_S *stream)
{
    uint32_t size = 0;
    uint32_t flags = 0;
    for (uint32_t index = 0; index < stream->u32PackCount; index++) {
        const VENC_PACK_S *pack = &stream->pstPack[index];
        if (!pack->pu8Addr || pack->u32Offset > pack->u32Len ||
            pack->u32Len - pack->u32Offset > BF01_VIDEO_MAX_FRAME - size)
            return EOVERFLOW;
        size += pack->u32Len - pack->u32Offset;
        if (pack->DataType.enH265EType == H265E_NALU_IDRSLICE ||
            pack->DataType.enH265EType == H265E_NALU_ISLICE)
            flags |= BF01_VIDEO_FLAG_KEY;
    }
    if (!size)
        return EPROTO;
    uint32_t newest = video_ring->write_sequence;
    uint32_t oldest = video_ring->oldest_sequence;
    uint32_t offset = video_offset;
    if (offset > BF01_VIDEO_DATA_SIZE - size)
        offset = 0;
    video_ring->generation++;
    flush_region(video_ring, offsetof(struct bf01_video_ring, frames));
    __sync_synchronize();
    if (newest - oldest >= BF01_VIDEO_FRAME_CAPACITY)
        oldest = newest - BF01_VIDEO_FRAME_CAPACITY + 1;
    for (uint32_t sequence = oldest; sequence < newest; sequence++) {
        const struct bf01_video_frame *previous = &video_ring->frames[sequence % BF01_VIDEO_FRAME_CAPACITY];
        if (offset < previous->offset + previous->size && previous->offset < offset + size)
            oldest = sequence + 1;
    }
    uint32_t copied = 0;
    for (uint32_t index = 0; index < stream->u32PackCount; index++) {
        const VENC_PACK_S *pack = &stream->pstPack[index];
        uint32_t length = pack->u32Len - pack->u32Offset;
        memcpy(video_ring->data + offset + copied, pack->pu8Addr + pack->u32Offset, length);
        copied += length;
    }
    flush_region(video_ring->data + offset, size);
    uint64_t pts = stream->pstPack[0].u64PTS;
    struct bf01_video_frame *frame = &video_ring->frames[newest % BF01_VIDEO_FRAME_CAPACITY];
    *frame = (struct bf01_video_frame) {
        .sequence = newest,
        .offset = offset,
        .size = size,
        .flags = flags,
        .pts_low = (uint32_t)pts,
        .pts_high = (uint32_t)(pts >> 32)
    };
    flush_region(frame, sizeof(*frame));
    video_offset = offset + size;
    video_ring->oldest_sequence = oldest;
    video_ring->write_sequence = newest + 1;
    __sync_synchronize();
    video_ring->generation++;
    flush_region(video_ring, offsetof(struct bf01_video_ring, frames));
    if (!newest || (newest + 1) % BF01_VIDEO_FPS == 0)
        printf("[boot-video] frames=%u oldest=%u pts=%llu boot_ms=%llu bytes=%u key=%u\n",
            newest + 1, oldest, (unsigned long long)pts,
            (unsigned long long)(csi_tick_get_us() / 1000), size, flags);
    return 0;
}

static int feed_frame(uint64_t *pts)
{
    VIDEO_FRAME_INFO_S input = {0};
    *pts = 0;
    CVI_S32 result = CVI_VI_GetChnFrame(0, 0, &input, VIDEO_TIMEOUT_MS);
    if (result != CVI_SUCCESS)
        goto failed;
    if (input.stVFrame.u64PTS < APP_BootAudioFirstPts()) {
        result = CVI_VI_ReleaseChnFrame(0, 0, &input);
        return result == CVI_SUCCESS ? 0 : EIO;
    }
    result = CVI_VPSS_SendFrame(1, &input, VIDEO_TIMEOUT_MS);
    CVI_S32 released = CVI_VI_ReleaseChnFrame(0, 0, &input);
    if (result != CVI_SUCCESS || released != CVI_SUCCESS)
        goto failed;
    *pts = input.stVFrame.u64PTS;
    return 0;
failed:
    printf("[boot-video] frame pipeline failed: ret=%#x\n", (unsigned int)result);
    return EIO;
}

static void *feed_video(void *opaque)
{
    int error = 0;
    uint32_t frames = 0;
    while (!stopping()) {
        uint64_t pts;
        error = feed_frame(&pts);
        if (error)
            break;
        if (pts && (++frames == 1 || frames % BF01_VIDEO_FPS == 0))
            printf("[boot-video] input frames=%u pts=%llu boot_ms=%llu\n",
                frames, (unsigned long long)pts,
                (unsigned long long)(csi_tick_get_us() / 1000));
    }
    pthread_mutex_lock(&video_mutex);
    *(int *)opaque = error;
    if (error)
        video_stop = 1;
    pthread_mutex_unlock(&video_mutex);
    return NULL;
}

static int receive_frame(void)
{
    VIDEO_FRAME_INFO_S output = {0};
    CVI_S32 result = CVI_VPSS_GetChnFrame(1, 0, &output, VIDEO_TIMEOUT_MS);
    if (result != CVI_SUCCESS)
        goto failed;
    result = CVI_VENC_SendFrame(VIDEO_CHANNEL, &output, VIDEO_TIMEOUT_MS);
    CVI_S32 released = CVI_VPSS_ReleaseChnFrame(1, 0, &output);
    if (result != CVI_SUCCESS || released != CVI_SUCCESS)
        goto failed;
    VENC_CHN_STATUS_S status = {0};
    uint32_t started_ms = (uint32_t)aos_now_ms();
    do {
        result = CVI_VENC_QueryStatus(VIDEO_CHANNEL, &status);
        if (result != CVI_SUCCESS)
            goto failed;
        if (status.u32CurPacks)
            break;
        if (stopping() || (uint32_t)aos_now_ms() - started_ms >= VIDEO_TIMEOUT_MS)
            return stopping() ? ECANCELED : ETIMEDOUT;
        aos_msleep(1);
    } while (1);
    if (status.u32CurPacks > VIDEO_PACK_CAPACITY)
        return EOVERFLOW;
    VENC_PACK_S packs[VIDEO_PACK_CAPACITY];
    VENC_STREAM_S stream = {.pstPack = packs, .u32PackCount = status.u32CurPacks};
    do {
        result = CVI_VENC_GetStream(VIDEO_CHANNEL, &stream, 100);
        if (result != CVI_ERR_VENC_BUSY)
            break;
        if (stopping() || (uint32_t)aos_now_ms() - started_ms >= VIDEO_TIMEOUT_MS)
            return stopping() ? ECANCELED : ETIMEDOUT;
        aos_msleep(1);
        stream.u32PackCount = status.u32CurPacks;
    } while (1);
    if (result != CVI_SUCCESS)
        goto failed;
    int error = !stream.u32PackCount || stream.u32PackCount > status.u32CurPacks ?
        EPROTO : save_stream(&stream);
    result = CVI_VENC_ReleaseStream(VIDEO_CHANNEL, &stream);
    return result == CVI_SUCCESS ? error : EIO;
failed:
    printf("[boot-video] stream receive failed: ret=%#x\n", (unsigned int)result);
    return EIO;
}

static void *record_video(void *opaque)
{
    (void)opaque;
    int created = 0;
    int receiving = 0;
    int unbound = 0;
    int audio_started = 0;
    int feeding = 0;
    int feed_error = 0;
    pthread_t feed_thread;
    int error = EIO;
    MMF_CHN_S source = {.enModId = CVI_ID_VPSS, .s32DevId = 1, .s32ChnId = 0};
    MMF_CHN_S jpeg = {.enModId = CVI_ID_VENC, .s32DevId = 0, .s32ChnId = 3};
    VENC_CHN_ATTR_S attributes = {0};
    attributes.stVencAttr.enType = PT_H265;
    attributes.stVencAttr.u32MaxPicWidth = BF01_VIDEO_WIDTH;
    attributes.stVencAttr.u32MaxPicHeight = BF01_VIDEO_HEIGHT;
    attributes.stVencAttr.u32PicWidth = BF01_VIDEO_WIDTH;
    attributes.stVencAttr.u32PicHeight = BF01_VIDEO_HEIGHT;
    attributes.stVencAttr.u32BufSize = 2U * 1024 * 1024;
    attributes.stVencAttr.bByFrame = CVI_TRUE;
    attributes.stVencAttr.bEsBufQueueEn = CVI_FALSE;
    attributes.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
    attributes.stRcAttr.stH265Cbr.u32Gop = BF01_VIDEO_FPS;
    attributes.stRcAttr.stH265Cbr.u32StatTime = 1;
    attributes.stRcAttr.stH265Cbr.u32SrcFrameRate = BF01_VIDEO_FPS;
    attributes.stRcAttr.stH265Cbr.fr32DstFrameRate = BF01_VIDEO_FPS;
    attributes.stRcAttr.stH265Cbr.u32BitRate = 4000;
    attributes.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    attributes.stGopAttr.stNormalP.s32IPQpDelta = 2;
    struct bf01_video_ring *allocated = NULL;
    CVI_S32 result = CVI_SYS_IonAlloc_Cached(&video_physical, (void **)&allocated,
        "bf01-boot-video", bf01_video_ring_size());
    if (result != CVI_SUCCESS || !allocated) {
        error = ENOMEM;
        goto done;
    }
    if (video_physical > UINT32_MAX) {
        CVI_SYS_IonFree(video_physical, allocated);
        error = EOVERFLOW;
        goto done;
    }
    memset(allocated, 0, offsetof(struct bf01_video_ring, data));
    memset(&allocated->audio, 0, sizeof(allocated->audio));
    allocated->magic = BF01_VIDEO_MAGIC;
    allocated->version = BF01_VIDEO_VERSION;
    allocated->width = BF01_VIDEO_WIDTH;
    allocated->height = BF01_VIDEO_HEIGHT;
    allocated->fps = BF01_VIDEO_FPS;
    allocated->codec = BF01_VIDEO_CODEC_H265;
    allocated->frame_capacity = BF01_VIDEO_FRAME_CAPACITY;
    allocated->data_size = BF01_VIDEO_DATA_SIZE;
    allocated->state = BF01_VIDEO_STARTING;
    pthread_mutex_lock(&video_mutex);
    video_ring = allocated;
    flush_region(video_ring, offsetof(struct bf01_video_ring, data));
    video_status.ring_paddr = (uint32_t)video_physical;
    video_status.ring_size = bf01_video_ring_size();
    pthread_mutex_unlock(&video_mutex);
    result = CVI_VENC_CreateChn(VIDEO_CHANNEL, &attributes);
    if (result != CVI_SUCCESS)
        goto done;
    created = 1;
    VENC_RECV_PIC_PARAM_S receive = {.s32RecvPicNum = -1};
    result = CVI_VENC_StartRecvFrame(VIDEO_CHANNEL, &receive);
    if (result != CVI_SUCCESS)
        goto done;
    receiving = 1;
    result = CVI_SYS_UnBind(&source, &jpeg);
    if (result != CVI_SUCCESS)
        goto done;
    unbound = 1;
    VIDEO_FRAME_INFO_S stale = {0};
    while (CVI_VPSS_GetChnFrame(1, 0, &stale, 0) == CVI_SUCCESS)
        CVI_VPSS_ReleaseChnFrame(1, 0, &stale);
    error = APP_BootAudioStart(&video_ring->audio,
        video_physical + offsetof(struct bf01_video_ring, audio));
    if (error)
        goto done;
    audio_started = 1;
    pthread_attr_t thread_attributes;
    error = pthread_attr_init(&thread_attributes);
    if (!error) {
        error = pthread_attr_setstacksize(&thread_attributes, 32U * 1024);
        if (!error)
            error = pthread_create(&feed_thread, &thread_attributes, feed_video, &feed_error);
        pthread_attr_destroy(&thread_attributes);
    }
    if (error)
        goto done;
    feeding = 1;
    set_state(BF01_VIDEO_CAPTURING, 0);
    printf("[boot-video] start: boot_ms=%llu channel=%u H265 %ux%u fps=%u ring=%#x bytes=%u pipelined=1\n",
        (unsigned long long)(csi_tick_get_us() / 1000), VIDEO_CHANNEL,
        BF01_VIDEO_WIDTH, BF01_VIDEO_HEIGHT, BF01_VIDEO_FPS,
        video_status.ring_paddr, video_status.ring_size);
    uint32_t started_ms = (uint32_t)aos_now_ms();
    error = 0;
    while (!stopping() && (uint32_t)aos_now_ms() - started_ms < VIDEO_MAX_SECONDS * 1000U) {
        error = APP_BootAudioError();
        if (error)
            break;
        error = receive_frame();
        if (error)
            break;
    }
    if (error == ECANCELED)
        error = 0;
done:
    if (feeding) {
        pthread_mutex_lock(&video_mutex);
        video_stop = 1;
        pthread_mutex_unlock(&video_mutex);
        pthread_join(feed_thread, NULL);
        if (!error)
            error = feed_error;
    }
    if (audio_started) {
        int audio_result = APP_BootAudioStop();
        if (!error)
            error = audio_result;
    }
    if (receiving && CVI_VENC_StopRecvFrame(VIDEO_CHANNEL) != CVI_SUCCESS)
        error = EIO;
    if (created && CVI_VENC_DestroyChn(VIDEO_CHANNEL) != CVI_SUCCESS)
        error = EIO;
    if (unbound && CVI_SYS_Bind(&source, &jpeg) != CVI_SUCCESS)
        error = EIO;
    printf("[boot-video] stopped: error=%d driver_ret=%#x frames=%u boot_ms=%llu\n",
        error, (unsigned int)result, video_ring ? video_ring->write_sequence : 0,
        (unsigned long long)(csi_tick_get_us() / 1000));
    set_state(error ? BF01_VIDEO_FAILED : BF01_VIDEO_READY, error);
    return NULL;
}

void APP_BootVideoStart(void)
{
    pthread_mutex_lock(&video_mutex);
    if (video_started || video_stop) {
        pthread_mutex_unlock(&video_mutex);
        return;
    }
    video_started = 1;
    pthread_mutex_unlock(&video_mutex);
    pthread_t thread;
    pthread_attr_t attributes;
    int result = pthread_attr_init(&attributes);
    if (!result) {
        result = pthread_attr_setstacksize(&attributes, 32U * 1024);
        if (!result)
            result = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        if (!result)
            result = pthread_create(&thread, &attributes, record_video, NULL);
        pthread_attr_destroy(&attributes);
    }
    if (result)
        set_state(BF01_VIDEO_FAILED, result > 0 ? result : EAGAIN);
}
