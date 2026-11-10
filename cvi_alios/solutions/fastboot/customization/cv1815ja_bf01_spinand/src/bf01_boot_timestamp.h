#ifndef BF01_BOOT_TIMESTAMP_H
#define BF01_BOOT_TIMESTAMP_H

#define BF01_TIMESTAMP_CLOCK_PORT 5
#define BF01_TIMESTAMP_CLOCK_REPLY_PORT 6
#define BF01_TIMESTAMP_CLOCK_SET 1
#define BF01_TIMESTAMP_CLOCK_SET_ACK 2
#define BF01_TIMESTAMP_MIN_SECONDS 1577836800ULL
#define BF01_TIMESTAMP_MAX_SECONDS 4102444800ULL

void APP_BootTimestampPrepare(int vpss_group, unsigned int width, unsigned int height);
void APP_BootTimestampInitClockSync(void);
void APP_BootTimestampStart(int vpss_group, unsigned int width, unsigned int height);
void APP_BootTimestampUpdate(void);
void APP_BootTimestampStop(void);

#endif
