#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <drv/iic.h>
#include <drv/tick.h>
#include <drv/cvi_irq.h>
#include <mmio.h>
#include "dw_iic_ll.h"
#include "bf01_boot_rtc.h"
#include "bf01_boot_timestamp.h"
#include "bf01_boot_timestamp_config.h"

#define BM8563_BUS 1U
#define BM8563_ADDRESS 0x51U
#define BM8563_REGISTER_COUNT 9U
#define BM8563_TIMEOUT_US 20000U
#define BM8563_READ_RESTART (1U << 10)
#define BM8563_CLOCK_ENABLE 0x0300200cU
#define BM8563_CLOCK_BIT (1U << 18)
#define BM8563_RESET_CONTROL 0x03003000U
#define BM8563_RESET_BIT (1U << 28)

static void reset_controller(void)
{
    size_t interrupt_state = csi_irq_save();
    mmio_clrbits_32(BM8563_RESET_CONTROL, BM8563_RESET_BIT);
    uint64_t started_us = csi_tick_get_us();
    while (csi_tick_get_us() - started_us < 2U) {}
    mmio_setbits_32(BM8563_RESET_CONTROL, BM8563_RESET_BIT);
    csi_irq_restore(interrupt_state);
}

static int decode_bcd(uint8_t value, unsigned int minimum, unsigned int maximum)
{
    unsigned int decoded = (value >> 4) * 10U + (value & 0x0fU);
    if ((value & 0x0fU) > 9U || (value >> 4) > 9U || decoded < minimum || decoded > maximum)
        return -1;
    return (int)decoded;
}

static int decode_time(const uint8_t *registers, unsigned long *seconds)
{
    if ((registers[0] & 0x20U) || (registers[2] & 0x80U))
        return ENODATA;
    int second = decode_bcd(registers[2] & 0x7fU, 0, 59);
    int minute = decode_bcd(registers[3] & 0x7fU, 0, 59);
    int hour = decode_bcd(registers[4] & 0x3fU, 0, 23);
    int day = decode_bcd(registers[5] & 0x3fU, 1, 31);
    int month = decode_bcd(registers[7] & 0x1fU, 1, 12);
    int year = decode_bcd(registers[8], 0, 99);
    if (second < 0 || minute < 0 || hour < 0 || day < 0 || month < 0 || year < 0 ||
        (registers[6] & 0x07U) > 6U)
        return EPROTO;
    year += registers[7] & 0x80U ? 1900 : 2000;
    if (year < 2020)
        return ERANGE;
    const unsigned int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned int leap = !(year % 4) && (year % 100 || !(year % 400));
    if ((unsigned int)day > month_days[month - 1] + (month == 2 ? leap : 0U))
        return EPROTO;
    uint64_t days = 0;
    for (int previous_year = 1970; previous_year < year; previous_year++)
        days += 365U + (!(previous_year % 4) && (previous_year % 100 || !(previous_year % 400)));
    for (int previous_month = 1; previous_month < month; previous_month++)
        days += month_days[previous_month - 1] + (previous_month == 2 ? leap : 0U);
    days += day - 1;
    int64_t epoch = (int64_t)days * 86400 + hour * 3600 + minute * 60 + second -
        (int64_t)BF01_EXTERNAL_RTC_UTC_OFFSET_MINUTES * 60;
    if (epoch < (int64_t)BF01_TIMESTAMP_MIN_SECONDS || epoch >= (int64_t)BF01_TIMESTAMP_MAX_SECONDS)
        return ERANGE;
    *seconds = (unsigned long)epoch;
    return 0;
}

static int wait_status(dw_iic_regs_t *controller, uint32_t mask, int interrupt_status, uint64_t started_us)
{
    for (;;) {
        if (controller->IC_RAW_INTR_STAT & DW_IIC_RAW_TX_ABRT)
            return EIO;
        uint32_t status = interrupt_status ? controller->IC_RAW_INTR_STAT : controller->IC_STATUS;
        if (status & mask)
            return 0;
        if (csi_tick_get_us() - started_us >= BM8563_TIMEOUT_US)
            return ETIMEDOUT;
    }
}

static int read_registers(csi_iic_t *bus, uint8_t *registers, uint64_t started_us)
{
    dw_iic_regs_t *controller = (dw_iic_regs_t *)HANDLE_REG_BASE(bus);
    dw_iic_set_target_address(controller, BM8563_ADDRESS);
    dw_iic_clear_all_irq(controller);
    dw_iic_enable(controller);
    int error = wait_status(controller, DW_IIC_TXFIFO_NOT_FULL_STATE, 0, started_us);
    if (!error)
        dw_iic_transmit_data(controller, 0x00U);
    for (unsigned int index = 0; index < BM8563_REGISTER_COUNT && !error; index++) {
        error = wait_status(controller, DW_IIC_TXFIFO_NOT_FULL_STATE, 0, started_us);
        if (error)
            break;
        uint32_t command = DW_IIC_DATA_CMD;
        if (index == 0)
            command |= BM8563_READ_RESTART;
        if (index == BM8563_REGISTER_COUNT - 1U)
            command |= DW_IIC_DATA_STOP;
        dw_iic_transmit_data(controller, command);
    }
    for (unsigned int index = 0; index < BM8563_REGISTER_COUNT && !error; index++) {
        error = wait_status(controller, DW_IIC_RXFIFO_NOT_EMPTY_STATE, 0, started_us);
        if (!error)
            registers[index] = (uint8_t)dw_iic_receive_data(controller);
    }
    if (!error)
        error = wait_status(controller, DW_IIC_RAW_STOP_DET, 1, started_us);
    if (error)
        printf("[boot-rtc] IIC1 read failed: addr=0x51 errno=%d abort=0x%08x status=0x%08x\n",
            error, controller->IC_TX_ABRT_SOURCE, controller->IC_STATUS);
    return error;
}

int APP_BootRtcRead(unsigned long *seconds)
{
    if (!seconds)
        return EINVAL;
    uint64_t started_us = csi_tick_get_us();
    size_t interrupt_state = csi_irq_save();
    mmio_setbits_32(BM8563_CLOCK_ENABLE, BM8563_CLOCK_BIT);
    mmio_setbits_32(BM8563_RESET_CONTROL, BM8563_RESET_BIT);
    csi_irq_restore(interrupt_state);
    csi_iic_t bus = {0};
    int result = csi_iic_init(&bus, BM8563_BUS);
    if (result != CSI_OK) {
        printf("[boot-rtc] IIC1 init failed: ret=%d\n", result);
        return EIO;
    }
    result = csi_iic_mode(&bus, IIC_MODE_MASTER);
    if (result == CSI_OK)
        result = csi_iic_addr_mode(&bus, IIC_ADDRESS_7BIT);
    if (result == CSI_OK)
        result = csi_iic_speed(&bus, IIC_BUS_SPEED_STANDARD);
    uint8_t registers[BM8563_REGISTER_COUNT] = {0};
    int error = result == CSI_OK ? read_registers(&bus, registers, started_us) : EIO;
    if (error)
        reset_controller();
    dw_iic_regs_t *controller = (dw_iic_regs_t *)HANDLE_REG_BASE((&bus));
    csi_iic_uninit(&bus);
    controller->IC_ENABLE = 0;
    if (!error) {
        printf("[boot-rtc] BM8563 IIC1 addr=0x51 reg00-08=%02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            registers[0], registers[1], registers[2], registers[3], registers[4],
            registers[5], registers[6], registers[7], registers[8]);
        error = decode_time(registers, seconds);
        if (error)
            printf("[boot-rtc] calendar rejected: STOP=%u VL=%u errno=%d; RTC initialization required\n",
                !!(registers[0] & 0x20U), !!(registers[2] & 0x80U), error);
    }
    if (!error)
        printf("[boot-rtc] time=%04d-%02d-%02d %02d:%02d:%02d weekday=%u stored_utc_offset_minutes=%d utc_seconds=%lu\n",
            2000 + decode_bcd(registers[8], 0, 99),
            decode_bcd(registers[7] & 0x1fU, 1, 12), decode_bcd(registers[5] & 0x3fU, 1, 31),
            decode_bcd(registers[4] & 0x3fU, 0, 23), decode_bcd(registers[3] & 0x7fU, 0, 59),
            decode_bcd(registers[2] & 0x7fU, 0, 59), registers[6] & 0x07U,
            BF01_EXTERNAL_RTC_UTC_OFFSET_MINUTES, *seconds);
    printf("[boot-rtc] read complete: errno=%d elapsed_us=%llu boot_ms=%llu; IIC1 released\n",
        error, (unsigned long long)(csi_tick_get_us() - started_us),
        (unsigned long long)(csi_tick_get_us() / 1000U));
    return error;
}
