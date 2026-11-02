#ifndef BF01_BOOT_CAPTURE_H
#define BF01_BOOT_CAPTURE_H

#include <stdint.h>
#include <stddef.h>

#define BF01_BURST_PORT 2
#define BF01_BURST_MAGIC 0x42465333U
#define BF01_BURST_VERSION 1
#define BF01_BURST_COUNT 3
#define BF01_BURST_INTERVAL_MS 500
#define BF01_BURST_WIDTH 1920
#define BF01_BURST_HEIGHT 1440
#define BF01_BURST_MAX_JPEG (2U * 1024 * 1024)
#define BF01_BURST_CHUNK 1024

enum bf01_burst_command {
    BF01_BURST_STATUS = 1,
    BF01_BURST_READ,
    BF01_BURST_COMMIT
};

enum bf01_burst_state {
    BF01_BURST_STARTING,
    BF01_BURST_CAPTURING,
    BF01_BURST_READY,
    BF01_BURST_FAILED,
    BF01_BURST_RELEASED
};

struct bf01_burst_image {
    uint32_t size;
    uint32_t crc;
    uint32_t pts_low;
    uint32_t pts_high;
    uint32_t captured_ms;
};

struct bf01_burst_request {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    uint32_t command;
    uint32_t index;
    uint32_t offset;
};

struct bf01_burst_reply {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    uint32_t command;
    int32_t error;
    uint32_t state;
    uint32_t count;
    uint32_t index;
    uint32_t offset;
    uint32_t length;
    uint32_t width;
    uint32_t height;
    struct bf01_burst_image images[BF01_BURST_COUNT];
    unsigned char data[BF01_BURST_CHUNK];
};

static inline uint32_t bf01_burst_crc(uint32_t crc, const unsigned char *data, size_t length)
{
    for (size_t offset = 0; offset < length; offset++) {
        crc ^= data[offset];
        for (unsigned int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return crc;
}

#endif
