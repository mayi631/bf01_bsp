#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <aos/kernel.h>
#include <drv/codec.h>
#include "bf01_boot_audio.h"
#include "cvi_sys.h"

#define AUDIO_DMA_BYTES (BF01_AUDIO_SAMPLES * sizeof(int16_t) * 16U)
#define AUDIO_TIMEOUT_MS 1000U
#define AUDIO_DMA_CHANNEL 3U

static csi_dma_t audio_controller;
static csi_codec_t audio_codec;
static csi_codec_input_t audio_input;
static csi_ringbuf_t audio_buffer;
static csi_dma_ch_t audio_dma;
static unsigned char dma_buffer[AUDIO_DMA_BYTES] __attribute__((aligned(64)));
static struct bf01_audio_ring *audio_ring;
static CVI_U64 audio_physical;
static uint64_t audio_first_pts;
static pthread_t audio_thread;
static volatile int audio_stop;
static volatile int audio_error;
static int codec_initialized;
static int controller_initialized;
static int input_open;
static int dma_linked;
static int input_started;
static int thread_started;

static void flush_audio(void *address, uint32_t size)
{
    CVI_SYS_IonFlushCache(audio_physical + ((unsigned char *)address - (unsigned char *)audio_ring),
        address, size);
}

static void audio_event(csi_codec_input_t *input, csi_codec_event_t event, void *opaque)
{
    (void)input;
    (void)opaque;
    if (event == CODEC_EVENT_READ_BUFFER_FULL || event == CODEC_EVENT_ERROR_OVERFLOW)
        audio_error = EOVERFLOW;
    else if (event == CODEC_EVENT_ERROR || event == CODEC_EVENT_ERROR_UNDERFLOW)
        audio_error = EIO;
}

static void *record_audio(void *opaque)
{
    (void)opaque;
    int16_t samples[BF01_AUDIO_SAMPLES];
    uint64_t sample_count = 0;
    uint32_t last_data_ms = (uint32_t)aos_now_ms();
    while (!audio_stop && !audio_error) {
        uint32_t count = csi_codec_input_read_async(&audio_input, samples, sizeof(samples));
        if (!count) {
            if ((uint32_t)aos_now_ms() - last_data_ms >= AUDIO_TIMEOUT_MS)
                audio_error = ETIMEDOUT;
            aos_msleep(1);
            continue;
        }
        if (count != sizeof(samples)) {
            audio_error = EIO;
            break;
        }
        last_data_ms = (uint32_t)aos_now_ms();
        uint32_t sequence = audio_ring->write_sequence;
        uint64_t pts = audio_first_pts + sample_count * 1000000ULL / BF01_AUDIO_RATE;
        audio_ring->generation++;
        flush_audio(audio_ring, offsetof(struct bf01_audio_ring, frames));
        __sync_synchronize();
        struct bf01_audio_frame *frame = &audio_ring->frames[sequence % BF01_AUDIO_FRAME_CAPACITY];
        frame->sequence = sequence;
        frame->count = BF01_AUDIO_SAMPLES;
        frame->pts_low = (uint32_t)pts;
        frame->pts_high = (uint32_t)(pts >> 32);
        memcpy(frame->samples, samples, sizeof(samples));
        flush_audio(frame, sizeof(*frame));
        audio_ring->oldest_sequence = sequence >= BF01_AUDIO_FRAME_CAPACITY ?
            sequence - BF01_AUDIO_FRAME_CAPACITY + 1 : 0;
        audio_ring->write_sequence = sequence + 1;
        audio_ring->state = BF01_VIDEO_CAPTURING;
        __sync_synchronize();
        audio_ring->generation++;
        flush_audio(audio_ring, offsetof(struct bf01_audio_ring, frames));
        sample_count += BF01_AUDIO_SAMPLES;
    }
    audio_ring->error = audio_error;
    audio_ring->state = audio_error ? BF01_VIDEO_FAILED : BF01_VIDEO_READY;
    flush_audio(audio_ring, offsetof(struct bf01_audio_ring, frames));
    printf("[boot-audio] stopped: error=%d samples=%llu frames=%u\n", audio_error,
        (unsigned long long)sample_count, audio_ring->write_sequence);
    return NULL;
}

int APP_BootAudioError(void)
{
    return audio_error;
}

uint64_t APP_BootAudioFirstPts(void)
{
    return audio_first_pts;
}

int APP_BootAudioStop(void)
{
    audio_stop = 1;
    if (thread_started) {
        pthread_join(audio_thread, NULL);
        thread_started = 0;
    }
    if (input_started) {
        csi_codec_input_stop(&audio_input);
        input_started = 0;
    }
    if (dma_linked) {
        csi_codec_input_link_dma(&audio_input, NULL);
        dma_linked = 0;
    }
    if (controller_initialized) {
        csi_dma_uninit(&audio_controller);
        controller_initialized = 0;
    }
    if (input_open) {
        csi_codec_input_detach_callback(&audio_input);
        csi_codec_input_close(&audio_input);
        input_open = 0;
    }
    if (codec_initialized) {
        csi_codec_uninit(&audio_codec);
        codec_initialized = 0;
    }
    if (audio_ring) {
        audio_ring->error = audio_error;
        audio_ring->state = audio_error ? BF01_VIDEO_FAILED : BF01_VIDEO_READY;
        flush_audio(audio_ring, offsetof(struct bf01_audio_ring, frames));
    }
    return audio_error;
}

int APP_BootAudioStart(struct bf01_audio_ring *ring, uint64_t physical)
{
    if (!ring || thread_started)
        return EINVAL;
    audio_ring = ring;
    audio_physical = physical;
    audio_stop = 0;
    audio_error = 0;
    audio_ring->rate = BF01_AUDIO_RATE;
    audio_ring->channels = 1;
    audio_ring->frame_capacity = BF01_AUDIO_FRAME_CAPACITY;
    audio_ring->state = BF01_VIDEO_STARTING;
    flush_audio(audio_ring, offsetof(struct bf01_audio_ring, frames));
    if (csi_codec_init(&audio_codec, 0) != CSI_OK)
        goto failed;
    codec_initialized = 1;
    audio_input.ring_buf = &audio_buffer;
    if (csi_codec_input_open(&audio_codec, &audio_input, 0) != CSI_OK)
        goto failed;
    input_open = 1;
    csi_codec_input_config_t config = {
        .sample_rate = BF01_AUDIO_RATE,
        .bit_width = 16,
        .mode = CODEC_INPUT_DIFFERENCE,
        .buffer = dma_buffer,
        .buffer_size = sizeof(dma_buffer),
        .period = BF01_AUDIO_SAMPLES * sizeof(int16_t),
        .sound_channel_num = 1,
    };
    if (csi_codec_input_config(&audio_input, &config) != CSI_OK ||
        csi_codec_input_analog_gain(&audio_input, 9) != CSI_OK ||
        csi_codec_input_attach_callback(&audio_input, audio_event, NULL) != CSI_OK)
        goto failed;
    if (csi_dma_init_shared(&audio_controller, 0, 1U << AUDIO_DMA_CHANNEL) != CSI_OK)
        goto failed;
    controller_initialized = 1;
    volatile uint32_t *remap = (volatile uint32_t *)0x03000154;
    *remap = (*remap & ~(0x7fU << 24)) | (1U << 31);
    if (csi_codec_input_link_dma(&audio_input, &audio_dma) != CSI_OK)
        goto failed;
    if (audio_dma.ch_id != AUDIO_DMA_CHANNEL) {
        csi_codec_input_link_dma(&audio_input, NULL);
        goto failed;
    }
    dma_linked = 1;
    CVI_U64 pts;
    if (CVI_SYS_GetCurPTS(&pts) != CVI_SUCCESS)
        goto failed;
    audio_first_pts = pts;
    if (csi_codec_input_start(&audio_input) != CSI_OK)
        goto failed;
    input_started = 1;
    pthread_attr_t attributes;
    int result = pthread_attr_init(&attributes);
    if (!result) {
        result = pthread_attr_setstacksize(&attributes, 16U * 1024);
        if (!result)
            result = pthread_create(&audio_thread, &attributes, record_audio, NULL);
        pthread_attr_destroy(&attributes);
    }
    if (result) {
        audio_error = result;
        goto failed;
    }
    thread_started = 1;
    uint32_t started_ms = (uint32_t)aos_now_ms();
    while (!audio_ring->write_sequence && !audio_error) {
        if ((uint32_t)aos_now_ms() - started_ms >= AUDIO_TIMEOUT_MS) {
            audio_error = ETIMEDOUT;
            break;
        }
        aos_msleep(1);
    }
    if (audio_error)
        goto failed;
    printf("[boot-audio] MIC ready: rate=%u channels=1 bits=16 first_pts=%llu; speaker off\n",
        BF01_AUDIO_RATE, (unsigned long long)audio_first_pts);
    return 0;
failed:
    if (!audio_error)
        audio_error = EIO;
    APP_BootAudioStop();
    printf("[boot-audio] MIC startup failed: error=%d\n", audio_error);
    return audio_error;
}
