#include "platform.h"
#include <drv/pin.h>
#include <pinctrl-mars.h>
#include <drv/adc.h>
#include "cvi_type.h"

#define GPIO_PIN_MASK(_gpio_num) (1 << _gpio_num)
#define BF01_PWR_ADC_LOW_CHECK_ENABLE 1

#if BF01_PWR_ADC_LOW_CHECK_ENABLE
#define BF01_PWR_ADC_INDEX 1U
#define BF01_PWR_ADC_CHANNEL 3U
#define BF01_PWR_ADC_REFERENCE_MV 1500U
#define BF01_PWR_ADC_SCALE 4096U
#define BF01_PWR_ADC_LOW_MV 0U

static int _ReadPwrAdc3(uint32_t *raw, uint32_t *mv)
{
	csi_adc_t adc = {0};
	csi_error_t ret;
	int32_t value;

	ret = csi_adc_init(&adc, BF01_PWR_ADC_INDEX);
	if (ret != CSI_OK) {
		printf("[BF01] PWR_VBAT_DET ADC init failed: %d\r\n", ret);
		if (adc.priv)
			csi_adc_uninit(&adc);
		return -1;
	}

	ret = csi_adc_channel_enable(&adc, BF01_PWR_ADC_CHANNEL, true);
	if (ret != CSI_OK) {
		printf("[BF01] PWR_VBAT_DET ADC channel failed: %d\r\n", ret);
		goto err_uninit;
	}

	csi_adc_trim(&adc);
	ret = csi_adc_start(&adc);
	if (ret != CSI_OK) {
		printf("[BF01] PWR_VBAT_DET ADC start failed: %d\r\n", ret);
		goto err_uninit;
	}

	value = csi_adc_read(&adc);
	csi_adc_stop(&adc);
	if (value < 0) {
		printf("[BF01] PWR_VBAT_DET ADC read failed: %d\r\n", value);
		goto err_uninit;
	}

	*raw = (uint32_t)value;
	*mv = *raw * BF01_PWR_ADC_REFERENCE_MV / BF01_PWR_ADC_SCALE;
	csi_adc_uninit(&adc);
	return 0;

err_uninit:
	csi_adc_uninit(&adc);
	return -1;
}

static void _PwrVbatCheck(void)
{
	uint32_t raw;
	uint32_t mv;

	PINMUX_CONFIG(PWR_VBAT_DET, PWR_VBAT_DET);
	if (_ReadPwrAdc3(&raw, &mv) != 0) {
		printf("[BF01] PWR_VBAT_DET check unavailable, continue boot\r\n");
		return;
	}

	printf("[BF01] PWR_VBAT_DET raw=%u, pin=%umV, cutoff=%umV\r\n",
	       raw, mv, BF01_PWR_ADC_LOW_MV);
	if (mv >= BF01_PWR_ADC_LOW_MV)
		return;

	printf("[BF01] low battery, requesting RTC poweroff\r\n");
	mmio_setbits_32(0x050260c0, 0x1);
	while ((mmio_read_32(0x050260c0) & 0x1) == 0)
		;
	mmio_write_32(0x05025004, 0xab18);
	while (1)
		mmio_write_32(0x05025008, 0x10001);
}
#endif

void _GPIOSetValue(u8 gpio_grp, u8 gpio_num, u8 level)
{
	csi_error_t ret;
	csi_gpio_t gpio = {0};

	ret = csi_gpio_init(&gpio, gpio_grp);
	if(ret != CSI_OK) {
		printf("csi_gpio_init failed\r\n");
		return;
	}
	// gpio write
	ret = csi_gpio_dir(&gpio , GPIO_PIN_MASK(gpio_num), GPIO_DIRECTION_OUTPUT);

	if(ret != CSI_OK) {
		printf("csi_gpio_dir failed\r\n");
		return;
	}
	csi_gpio_write(&gpio , GPIO_PIN_MASK(gpio_num), level);
	//printf("test pin end and success.\r\n");
}
void PLATFORM_SpkMute(int value)
{
	u8 gpio_spken_r_grp = 4;
	u8 gpio_spken_r_num = 2;
#if defined (CONFIG_CHIP_cv1811h) || defined (CONFIG_CHIP_cv1812h) || (CONFIG_CHIP_cv1811ha) || (CONFIG_CHIP_cv1812ha) || (CONFIG_CHIP_cv1813h)
	u8 gpio_spken_l_grp;
	u8 gpio_spken_l_num;
#endif

#if defined (CONFIG_CHIP_cv1811c) || defined (CONFIG_CHIP_cv1801c) || defined (CONFIG_CHIP_cv1810c) || defined (CONFIG_CHIP_cv1812cp)
	gpio_spken_r_grp = 4;
	gpio_spken_r_num = 2;
#endif
#if defined (CONFIG_CHIP_cv1801b) || defined (CONFIG_CHIP_cv180zb)
	gpio_spken_r_grp = 0;
	gpio_spken_r_num = 15;
#endif
#if defined (CONFIG_CHIP_cv1811h) || defined (CONFIG_CHIP_cv1812h) || (CONFIG_CHIP_cv1811ha) || (CONFIG_CHIP_cv1812ha) || (CONFIG_CHIP_cv1813h)
	gpio_spken_r_grp = 0;
	gpio_spken_r_num = 30;
	gpio_spken_l_grp = 0;
	gpio_spken_l_num = 15;
#endif
//0静音 ，1非静音
    if(value){
        _GPIOSetValue(gpio_spken_r_grp, gpio_spken_r_num, 1);
#if defined (CONFIG_CHIP_cv1811h) || defined (CONFIG_CHIP_cv1812h) || (CONFIG_CHIP_cv1811ha) || (CONFIG_CHIP_cv1812ha) || (CONFIG_CHIP_cv1813h)
        _GPIOSetValue(gpio_spken_l_grp, gpio_spken_l_num, 1);
#endif
    }else{
        _GPIOSetValue(gpio_spken_r_grp, gpio_spken_r_grp, 0);
#if defined (CONFIG_CHIP_cv1811h) || defined (CONFIG_CHIP_cv1812h) || (CONFIG_CHIP_cv1811ha) || (CONFIG_CHIP_cv1812ha) || (CONFIG_CHIP_cv1813h)
        _GPIOSetValue(gpio_spken_l_grp, gpio_spken_l_num, 0);
#endif
    }
}
static void _AudioPinmux(void)
{
#if defined (CONFIG_CHIP_cv1811c) || defined (CONFIG_CHIP_cv1801c) || defined (CONFIG_CHIP_cv1810c) || defined (CONFIG_CHIP_cv1812cp)
    PINMUX_CONFIG(PWR_GPIO2, PWR_GPIO_2);
#endif
#if defined (CONFIG_CHIP_cv1811h) || defined (CONFIG_CHIP_cv1812h) || (CONFIG_CHIP_cv1811ha) || (CONFIG_CHIP_cv1812ha) || (CONFIG_CHIP_cv1813h)
    PINMUX_CONFIG(SPK_EN, XGPIOA_15);
    PINMUX_CONFIG(AUX0, XGPIOA_30);
#endif
#if defined (CONFIG_CHIP_cv1801b) || defined (CONFIG_CHIP_cv180zb)
    PINMUX_CONFIG(SPK_EN, XGPIOA_15);
#endif
    PINMUX_CONFIG(SPK_EN, XGPIOA_15);
    _GPIOSetValue(0, 15, 0);
}
static void _UartPinmux()
{
	// uart1 pinmux
	PINMUX_CONFIG(IIC0_SCL, UART1_TX);
	PINMUX_CONFIG(IIC0_SDA, UART1_RX);
}

static void _SensorPinmux()
{
	/* BF01 */
	PINMUX_CONFIG(IIC3_SCL, IIC3_SCL);
	PINMUX_CONFIG(IIC3_SDA, IIC3_SDA);
	PINMUX_CONFIG(CAM_MCLK0, CAM_MCLK0);
	PINMUX_CONFIG(VIVO_D3, XGPIOB_18); /* S_PWDN */
	PINMUX_CONFIG(VIVO_D5, XGPIOB_16); /* S_RESET */

	/* default: S_PWDN to high,  S_RESET to low */
	_GPIOSetValue(1, 18, 1);
	_GPIOSetValue(1, 16, 0);
}

static void _MipiRxPinmux(void)
{

	PINMUX_CONFIG(PAD_MIPIRX0P, XGPIOC_11);
    PINMUX_CONFIG(PAD_MIPIRX0N, XGPIOC_10);
	PINMUX_CONFIG(PAD_MIPIRX1P, XGPIOC_9);
    PINMUX_CONFIG(PAD_MIPIRX1N, XGPIOC_8);
	PINMUX_CONFIG(PAD_MIPIRX2P, XGPIOC_7);
    PINMUX_CONFIG(PAD_MIPIRX2N, XGPIOC_6);
	PINMUX_CONFIG(PAD_MIPIRX3P, XGPIOC_5);
    PINMUX_CONFIG(PAD_MIPIRX3N, XGPIOC_4);
    PINMUX_CONFIG(PAD_MIPIRX4P, XGPIOC_3);
    PINMUX_CONFIG(PAD_MIPIRX4N, XGPIOC_2);
}

static void _MipiTxPinmux(void)
{
//mipi tx pinmux
#if CONFIG_PANEL_ILI9488
	PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
	PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
	PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
	PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
	PINMUX_CONFIG(IIC0_SCL, XGPIOA_28);
#elif (CONFIG_PANEL_HX8394)
#if CONFIG_BOARD_CV181XC
	PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
	PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
	PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
	PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
	PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
	PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
	PINMUX_CONFIG(JTAG_CPU_TCK, XGPIOA_18);
	PINMUX_CONFIG(JTAG_CPU_TMS, XGPIOA_19);
	PINMUX_CONFIG(SPK_EN, XGPIOA_15);
#elif (defined(__CV181X__))
	PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
	PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
	PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
	PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
	PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
	PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
	PINMUX_CONFIG(PAD_MIPI_TXM3, XGPIOC_20);
	PINMUX_CONFIG(PAD_MIPI_TXP3, XGPIOC_21);
	PINMUX_CONFIG(PAD_MIPI_TXM4, XGPIOC_18);
	PINMUX_CONFIG(PAD_MIPI_TXP4, XGPIOC_19);
#endif
#endif
}

#if (CONFIG_APP_DEBUG_JTAG == 1)
void JTAG_PinmuxIn()
{
    PINMUX_CONFIG(IIC0_SDA, CV_SDA0__CR_4WTDO);
    PINMUX_CONFIG(IIC0_SCL, CV_SCL0__CR_4WTDI);
}
#endif

void PLATFORM_IoInit(void)
{
#if BF01_PWR_ADC_LOW_CHECK_ENABLE
	_PwrVbatCheck();
#endif
//pinmux 切换接口
    _UartPinmux();
    _MipiRxPinmux();
    _MipiTxPinmux();
    _SensorPinmux();
    _AudioPinmux();
	#if (CONFIG_APP_DEBUG_JTAG == 1)
	JTAG_PinmuxIn();
	#endif

#if 0 //evb mipi switch
	PINMUX_CONFIG(SD1_CMD, IIC3_SCL);
	PINMUX_CONFIG(SD1_CLK, IIC3_SDA);
	PINMUX_CONFIG(ADC1, PWM_3);
#endif
}

void PLATFORM_PowerOff(void)
{
//下电休眠前调用接口
}

int PLATFORM_PanelInit(void)
{
    return CVI_SUCCESS;
}

void PLATFORM_PanelBacklightCtl(int level)
{

}

int PLATFORM_IrCutCtl(int duty)
{
    return 0;
}

/* Platform-specific pinmux configuration functions for IIC2 */
void PLATFORM_IIC2_ConfigGpioMode(void)
{
    PINMUX_CONFIG(IIC2_SCL, PWR_GPIO_12);  /* IIC2_SCL -> PWR_GPIO_12 */
    PINMUX_CONFIG(IIC2_SDA, PWR_GPIO_13);  /* IIC2_SDA -> PWR_GPIO_13 */
}

void PLATFORM_IIC2_ConfigIicMode(void)
{
    PINMUX_CONFIG(IIC2_SCL, IIC2_SCL);
    PINMUX_CONFIG(IIC2_SDA, IIC2_SDA);
}

/* Platform-specific pinmux configuration functions for IIC3 */
void PLATFORM_IIC3_ConfigGpioMode(void)
{
    PINMUX_CONFIG(IIC3_SCL, XGPIOA_5);
    PINMUX_CONFIG(IIC3_SDA, XGPIOA_6);
}

void PLATFORM_IIC3_ConfigIicMode(void)
{
    PINMUX_CONFIG(IIC3_SCL, IIC3_SCL);
    PINMUX_CONFIG(IIC3_SDA, IIC3_SDA);
}
