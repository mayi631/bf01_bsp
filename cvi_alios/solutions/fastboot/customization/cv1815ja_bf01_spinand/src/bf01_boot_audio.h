#ifndef BF01_BOOT_AUDIO_H
#define BF01_BOOT_AUDIO_H

#include "bf01_boot_video.h"

int APP_BootAudioStart(struct bf01_audio_ring *ring, uint64_t physical);
int APP_BootAudioError(void);
uint64_t APP_BootAudioFirstPts(void);
int APP_BootAudioStop(void);

#endif
