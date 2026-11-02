#ifndef BF01_BOOT_VIDEO_H
#define BF01_BOOT_VIDEO_H

#include <stddef.h>
#include <stdint.h>

#define BF01_VIDEO_PORT 3
#define BF01_VIDEO_MAGIC 0x42465631U
#define BF01_VIDEO_VERSION 2
#define BF01_VIDEO_WIDTH 1920
#define BF01_VIDEO_HEIGHT 1440
#define BF01_VIDEO_FPS 25
#define BF01_VIDEO_CODEC_H265 265U
#define BF01_VIDEO_DATA_SIZE (8U * 1024 * 1024)
#define BF01_VIDEO_FRAME_CAPACITY 512U
#define BF01_VIDEO_MAX_FRAME (1024U * 1024)
#define BF01_VIDEO_FLAG_KEY 1U
#define BF01_AUDIO_RATE 16000U
#define BF01_AUDIO_SAMPLES 320U
#define BF01_AUDIO_FRAME_CAPACITY 2048U

enum bf01_video_command {
    BF01_VIDEO_STATUS = 1,
    BF01_VIDEO_STOP,
    BF01_VIDEO_RELEASE
};

enum bf01_video_state {
    BF01_VIDEO_STARTING,
    BF01_VIDEO_CAPTURING,
    BF01_VIDEO_READY,
    BF01_VIDEO_FAILED,
    BF01_VIDEO_RELEASED
};

struct bf01_video_request {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    uint32_t command;
};

struct bf01_video_reply {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    int32_t error;
    uint32_t state;
    uint32_t ring_paddr;
    uint32_t ring_size;
};

struct bf01_video_frame {
    uint32_t sequence;
    uint32_t offset;
    uint32_t size;
    uint32_t flags;
    uint32_t pts_low;
    uint32_t pts_high;
    uint32_t reserved[2];
};

struct bf01_audio_frame {
    uint32_t sequence;
    uint32_t count;
    uint32_t pts_low;
    uint32_t pts_high;
    int16_t samples[BF01_AUDIO_SAMPLES];
};

struct bf01_audio_ring {
    uint32_t rate;
    uint32_t channels;
    uint32_t frame_capacity;
    uint32_t state;
    int32_t error;
    uint32_t oldest_sequence;
    uint32_t write_sequence;
    uint32_t generation;
    uint32_t reserved[8];
    struct bf01_audio_frame frames[BF01_AUDIO_FRAME_CAPACITY];
};

struct bf01_video_ring {
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t codec;
    uint32_t frame_capacity;
    uint32_t data_size;
    uint32_t state;
    uint32_t oldest_sequence;
    uint32_t write_sequence;
    uint32_t generation;
    uint32_t reserved[20];
    struct bf01_video_frame frames[BF01_VIDEO_FRAME_CAPACITY];
    unsigned char data[BF01_VIDEO_DATA_SIZE];
    struct bf01_audio_ring audio;
};

static inline size_t bf01_video_ring_size(void)
{
    return sizeof(struct bf01_video_ring);
}

int APP_BootVideoInit(void);
void APP_BootVideoStart(void);

#endif
