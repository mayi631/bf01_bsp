#include "platform.h"
#include <drv/pin.h>
#include <pinctrl-mars.h>
#include "cvi_type.h"
#include "mmio.h"
#include <stdio.h>
#include <aos/cli.h>

#define GPIO_PIN_MASK(_gpio_num) (1 << _gpio_num)

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
    PLATFORM_SpkMute(1);
}
static void _UartPinmux()
{
	// uart1 pinmux
	PINMUX_CONFIG(IIC0_SCL, UART1_TX);
	PINMUX_CONFIG(IIC0_SDA, UART1_RX);
}

#define BF01_MARK_REG   0x1901FE8U
#define BF01_MARK_BOOT   0xBF010001U
#define BF01_MARK_CLI    0xBF01BEEFU

static void _Bf01WriteMark(uint32_t magic)
{
	mmio_write_32(BF01_MARK_REG, magic);
}

static void _SensorCamMclk0Enable24M(void)
{
	/*
	 * Early CAM_MCLK0 = 24MHz so Sensor I2C can ACK before media init.
	 * clkgen base 0x03002000:
	 *   REG_CLK_CAM0_SRC_DIV (0x8C0) -> 0x030028C0
	 *   REG_CLK_EN_2 (0x008) bit16   -> 0x03002008
	 * CAMPLL_FREQ_24M packing matches cif: div_val=50, src_sel=2, div_val_sel=1
	 */
	uint32_t en;

	mmio_write_32(0x030028C0, 0x00320201);
	en = mmio_read_32(0x03002008);
	mmio_write_32(0x03002008, en | (1u << 16));
}

static void _SensorPinmux()
{
#if defined (CONFIG_CHIP_cv1815ja) || defined (CONFIG_CHIP_cv1815j)
	/* BF01: GC4683 on IIC2 + CAM_MCLK0, RST=XGPIOA[2], PWR=XGPIOA[3] */
	PINMUX_CONFIG(IIC2_SCL, IIC2_SCL);
	PINMUX_CONFIG(IIC2_SDA, IIC2_SDA);
	PINMUX_CONFIG(CAM_MCLK0, CAM_MCLK0);
	PINMUX_CONFIG(CAM_RST0, XGPIOA_2);   /* SENSOR_RST */
	PINMUX_CONFIG(CAM_MCLK1, XGPIOA_3);  /* SENSOR_PWR */

	/* Software IIC2 pull-up (board proven: timeout -> NACK) */
	mmio_write_32(0x05027040, 0x44); /* IIC2_SCL */
	mmio_write_32(0x05027044, 0x44); /* IIC2_SDA */

	/* Platform power + RST release + early MCLK.
	 * MEDIA_VIDEO_Init can still assert->MCLK->deassert via VIPARAM. */
	_GPIOSetValue(0, 3, 1); /* PWR high */
	_GPIOSetValue(0, 2, 1); /* RST deassert/high */
	_SensorCamMclk0Enable24M();
		_Bf01WriteMark(BF01_MARK_BOOT);
		printf("\r\n**** BF01_YOC_MARK=20260923_1535 GC4683 IIC2/MCLK0/RSTA2/PWR A3 ****\r\n");
		printf("**** CAM_MCLK0_DIV=0x%08x CLK_EN2=0x%08x ****\r\n",
		       mmio_read_32(0x030028C0), mmio_read_32(0x03002008));
#endif
}

static void _MipiRxPinmux(void)
{
//mipi rx pinmux
#if 0 //need porting for cv180x
    PINMUX_CONFIG(PAD_MIPIRX4P, XGPIOC_3);
    PINMUX_CONFIG(PAD_MIPIRX4N, XGPIOC_2);
#endif
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
//pinmux 切换接口
    printf("\r\n**** BF01 PLATFORM_IoInit START ****\r\n");
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
    printf("**** BF01 PLATFORM_IoInit DONE ****\r\n");
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


static void bf01_mark(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	_Bf01WriteMark(BF01_MARK_CLI);
	aos_cli_printf("\r\n#### BF01_ALIOS_BOOT_MARK=20260923_MCLK_IPCFIX ####\r\n");
	aos_cli_printf("**** BF01_YOC_MARK=20260923_1535 GC4683 IIC2/MCLK0/RSTA2/PWR A3 ****\r\n");
	aos_cli_printf("**** MARK_REG[0x1901FE8]=0x%08x ****\r\n", mmio_read_32(BF01_MARK_REG));
	aos_cli_printf("**** CAM_MCLK0_DIV=0x%08x CLK_EN2=0x%08x ****\r\n",
		       mmio_read_32(0x030028C0), mmio_read_32(0x03002008));
	aos_cli_printf("**** IIC2_PU SCL=0x%08x SDA=0x%08x ****\r\n",
		       mmio_read_32(0x05027040), mmio_read_32(0x05027044));
	aos_cli_printf("**** pinmux SCL=0x%08x SDA=0x%08x MCLK0=0x%08x RST=0x%08x PWR=0x%08x ****\r\n",
		       mmio_read_32(0x030010b8), mmio_read_32(0x030010bc),
		       mmio_read_32(0x03001000), mmio_read_32(0x03001008),
		       mmio_read_32(0x0300100c));
	aos_cli_printf("**** RTOS_INIT_MEDIA compile-time: %d ****\r\n",
#if defined(CONFIG_RTOS_INIT_MEDIA) && CONFIG_RTOS_INIT_MEDIA
		1
#else
		0
#endif
		);
}

ALIOS_CLI_CMD_REGISTER(bf01_mark, bf01_mark, print BF01 yoc mark and sensor regs);
