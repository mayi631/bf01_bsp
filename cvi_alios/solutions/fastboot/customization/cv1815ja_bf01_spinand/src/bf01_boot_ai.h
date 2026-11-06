#ifndef BF01_BOOT_AI_H
#define BF01_BOOT_AI_H

#include <stdint.h>

#define BF01_AI_PORT 4
#define BF01_AI_MAGIC 0x42464131U
#define BF01_AI_VERSION 1
#define BF01_AI_WIDTH 640U
#define BF01_AI_HEIGHT 640U
#define BF01_AI_IMAGE_SIZE (BF01_AI_WIDTH * BF01_AI_HEIGHT * 3U)
#define BF01_AI_TIMEOUT_MS 45000U

enum bf01_ai_command {
    BF01_AI_STATUS = 1,
    BF01_AI_ACCEPT,
    BF01_AI_REJECT
};

enum bf01_ai_state {
    BF01_AI_WAITING,
    BF01_AI_READY,
    BF01_AI_PASSED,
    BF01_AI_REJECTED,
    BF01_AI_FAILED
};

struct bf01_ai_request {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    uint32_t command;
    uint32_t image_paddr;
};

struct bf01_ai_reply {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    int32_t error;
    uint32_t state;
    uint32_t image_paddr;
    uint32_t image_size;
    uint32_t width;
    uint32_t height;
    uint32_t pts_low;
    uint32_t pts_high;
    uint32_t ion_base;
    uint32_t ion_size;
    uint32_t rtos_ion_size;
    uint32_t rtos_base;
    uint32_t rtos_size;
};

int APP_BootAiInit(void);
int APP_BootAiCapture(int publish, uint64_t expected_pts);
void APP_BootAiFail(int error);
int APP_BootAiRejected(void);

#endif
