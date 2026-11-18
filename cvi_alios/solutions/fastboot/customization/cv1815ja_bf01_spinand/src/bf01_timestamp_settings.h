#ifndef BF01_TIMESTAMP_SETTINGS_H
#define BF01_TIMESTAMP_SETTINGS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BF01_TS_RECORD_MAGIC 0x31535442U
#define BF01_TS_POLICY_MAGIC 0x42460000U
#define BF01_TS_BOOT_BYTES 4096U
#define BF01_TS_RECORD_OFFSET 2048U
#define BF01_TS_SLOT_BYTES 0x40000U
#define BF01_TS_SLOT0_OFFSET 0x08d80000U
#define BF01_TS_SLOT1_OFFSET 0x08dc0000U

struct bf01_ts_record {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t generation;
    uint32_t policy;
    uint32_t inverse_policy;
    uint32_t checksum;
    uint32_t reserved;
};

_Static_assert(sizeof(struct bf01_ts_record) == 32, "timestamp boot record ABI");

static inline uint32_t bf01_ts_crc(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint32_t checksum = 0xffffffffU;
    for (size_t index = 0; index < size; index++) {
        checksum ^= bytes[index];
        for (unsigned int bit = 0; bit < 8; bit++)
            checksum = (checksum >> 1) ^ (0xedb88320U & (0U - (checksum & 1U)));
    }
    return checksum ^ 0xffffffffU;
}

static inline int bf01_ts_policy_valid(uint32_t policy)
{
    return (policy & ~0x307U) == BF01_TS_POLICY_MAGIC && ((policy >> 1) & 3U) <= 2U;
}

static inline int bf01_ts_record_valid(const struct bf01_ts_record *record)
{
    return record->magic == BF01_TS_RECORD_MAGIC && record->version == 1 &&
        record->size == sizeof(*record) && record->generation &&
        bf01_ts_policy_valid(record->policy) && record->inverse_policy == ~record->policy &&
        !record->reserved && record->checksum == bf01_ts_crc(record, offsetof(struct bf01_ts_record, checksum));
}

static inline void bf01_ts_record_init(struct bf01_ts_record *record, uint32_t generation, uint32_t policy)
{
    memset(record, 0, sizeof(*record));
    record->magic = BF01_TS_RECORD_MAGIC;
    record->version = 1;
    record->size = sizeof(*record);
    record->generation = generation;
    record->policy = policy;
    record->inverse_policy = ~policy;
    record->checksum = bf01_ts_crc(record, offsetof(struct bf01_ts_record, checksum));
}

static inline int bf01_ts_record_choose(const struct bf01_ts_record records[2])
{
    int first = bf01_ts_record_valid(&records[0]);
    int second = bf01_ts_record_valid(&records[1]);
    if (!first)
        return second ? 1 : -1;
    if (!second)
        return 0;
    if (records[0].generation == records[1].generation && records[0].policy != records[1].policy)
        return -1;
    return records[1].generation > records[0].generation ? 1 : 0;
}

#endif
