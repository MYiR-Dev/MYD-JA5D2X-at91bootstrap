/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2016, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "common.h"
#include "hardware.h"
#include "pmc.h"
#include "usart.h"
#include "debug.h"
#include "ddramc.h"
#include "gpio.h"
#include "timer.h"
#include "watchdog.h"

#include "arch/at91_pmc.h"
#include "arch/at91_rstc.h"
#include "arch/at91_pio.h"
#include "arch/at91_ddrsdrc.h"
#include "arch/at91_sfr.h"
#include "arch/sama5_smc.h"
#include "l2cc.h"
#include "act8865.h"
#include "twi.h"
#include "arch/tz_matrix.h"
#include "matrix.h"
#include "sama5d2_ptc.h"

static void at91_dbgu_hw_init(void)
{
	const struct pio_desc dbgu_pins[] = {
		{"RXD0", CONFIG_SYS_DBGU_RXD_PIN, 0, PIO_DEFAULT, PIO_PERIPH_C},
		{"TXD0", CONFIG_SYS_DBGU_TXD_PIN, 0, PIO_DEFAULT, PIO_PERIPH_C},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};

	pio_configure(dbgu_pins);
	pmc_sam9x5_enable_periph_clk(CONFIG_SYS_DBGU_ID);
}

static void initialize_dbgu(void)
{
	unsigned int baudrate = 57600;

	at91_dbgu_hw_init();

	if (pmc_check_mck_h32mxdiv())
		usart_init(BAUDRATE(MASTER_CLOCK / 2, baudrate));
	else
		usart_init(BAUDRATE(MASTER_CLOCK, baudrate));
}

#if defined(CONFIG_MATRIX)
static int matrix_configure_slave(void)
{
	unsigned int ddr_port;
	unsigned int ssr_setting, sasplit_setting, srtop_setting;

	/*
	 * Matrix 0 (H64MX)
	 */

	/*
	 * 0: Bridge from H64MX to AXIMX
	 * (Internal ROM, Crypto Library, PKCC RAM): Always Secured
	 */

	/* 1: H64MX Peripheral Bridge */

	/* 2 ~ 9 DDR2 Port1 ~ 7: Non-Secure */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = (MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_128M) |
			   MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_128M) |
			   MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_128M) |
			   MATRIX_SASPLIT(3, MATRIX_SASPLIT_VALUE_128M));
	ssr_setting = (MATRIX_LANSECH_NS(0) |
		       MATRIX_LANSECH_NS(1) |
		       MATRIX_LANSECH_NS(2) |
		       MATRIX_LANSECH_NS(3) |
		       MATRIX_RDNSECH_NS(0) |
		       MATRIX_RDNSECH_NS(1) |
		       MATRIX_RDNSECH_NS(2) |
		       MATRIX_RDNSECH_NS(3) |
		       MATRIX_WRNSECH_NS(0) |
		       MATRIX_WRNSECH_NS(1) |
		       MATRIX_WRNSECH_NS(2) |
		       MATRIX_WRNSECH_NS(3));
	/* DDR port 0 not used from NWd */
	for (ddr_port = 1; ddr_port < 8; ddr_port++) {
		matrix_configure_slave_security(AT91C_BASE_MATRIX64,
					(H64MX_SLAVE_DDR2_PORT_0 + ddr_port),
					srtop_setting,
					sasplit_setting,
					ssr_setting);
	}

	/*
	 * 10: Internal SRAM 128K
	 * TOP0 is set to 128K
	 * SPLIT0 is set to 64K
	 * LANSECH0 is set to 0, the low area of region 0 is the Securable one
	 * RDNSECH0 is set to 0, region 0 Securable area is secured for reads.
	 * WRNSECH0 is set to 0, region 0 Securable area is secured for writes
	 */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_64K);
	ssr_setting = (MATRIX_LANSECH_S(0) |
		       MATRIX_RDNSECH_S(0) |
		       MATRIX_WRNSECH_S(0));
	matrix_configure_slave_security(AT91C_BASE_MATRIX64,
					H64MX_SLAVE_INTERNAL_SRAM,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 11:  Internal SRAM 128K (Cache L2) */
	/* 12:  QSPI0 */
	/* 13:  QSPI1 */
	/* 14:  AESB */

	/*
	 * Matrix 1 (H32MX)
	 */

	/* 0: Bridge from H32MX to H64MX: Not Secured */

	/* 1: H32MX Peripheral Bridge 0: Not Secured */

	/* 2: H32MX Peripheral Bridge 1: Not Secured */

	/*
	 * 3: External Bus Interface
	 * EBI CS0 Memory(256M) ----> Slave Region 0, 1
	 * EBI CS1 Memory(256M) ----> Slave Region 2, 3
	 * EBI CS2 Memory(256M) ----> Slave Region 4, 5
	 * EBI CS3 Memory(128M) ----> Slave Region 6
	 * NFC Command Registers(128M) -->Slave Region 7
	 *
	 * NANDFlash(EBI CS3) --> Slave Region 6: Non-Secure
	 */
	srtop_setting =	MATRIX_SRTOP(6, MATRIX_SRTOP_VALUE_128M);
	srtop_setting |= MATRIX_SRTOP(7, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(6, MATRIX_SASPLIT_VALUE_128M);
	sasplit_setting |= MATRIX_SASPLIT(7, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = (MATRIX_LANSECH_NS(6) |
		       MATRIX_RDNSECH_NS(6) |
		       MATRIX_WRNSECH_NS(6));
	ssr_setting |= (MATRIX_LANSECH_NS(7) |
			MATRIX_RDNSECH_NS(7) |
			MATRIX_WRNSECH_NS(7));
	matrix_configure_slave_security(AT91C_BASE_MATRIX32,
					H32MX_EXTERNAL_EBI,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 4: NFC SRAM (4K): Non-Secure */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_8K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_8K);
	ssr_setting = (MATRIX_LANSECH_NS(0) |
		       MATRIX_RDNSECH_NS(0) |
		       MATRIX_WRNSECH_NS(0));
	matrix_configure_slave_security(AT91C_BASE_MATRIX32,
					H32MX_NFC_SRAM,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 5:
	 * USB Device High Speed Dual Port RAM (DPR): 1M
	 * USB Host OHCI registers: 1M
	 * USB Host EHCI registers: 1M
	 */
	srtop_setting = (MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_1M) |
			 MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_1M) |
			 MATRIX_SRTOP(2, MATRIX_SRTOP_VALUE_1M));
	sasplit_setting = (MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_1M) |
			   MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_1M) |
			   MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_1M));
	ssr_setting = (MATRIX_LANSECH_NS(0) |
		       MATRIX_LANSECH_NS(1) |
		       MATRIX_LANSECH_NS(2) |
		       MATRIX_RDNSECH_NS(0) |
		       MATRIX_RDNSECH_NS(1) |
		       MATRIX_RDNSECH_NS(2) |
		       MATRIX_WRNSECH_NS(0) |
		       MATRIX_WRNSECH_NS(1) |
		       MATRIX_WRNSECH_NS(2));
	matrix_configure_slave_security(AT91C_BASE_MATRIX32,
					H32MX_USB,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	return 0;
}

static unsigned int security_ps_peri_id[] = {
	0,
};

static int matrix_config_periheral(void)
{
	unsigned int *peri_id = security_ps_peri_id;
	unsigned int array_size = sizeof(security_ps_peri_id) / sizeof(unsigned int);
	int ret;

	ret = matrix_configure_peri_security(peri_id, array_size);
	if (ret)
		return -1;

	return 0;
}

static int matrix_init(void)
{
	int ret;

	matrix_write_protect_disable(AT91C_BASE_MATRIX64);
	matrix_write_protect_disable(AT91C_BASE_MATRIX32);

	ret = matrix_configure_slave();
	if (ret)
		return -1;

	ret = matrix_config_periheral();
	if (ret)
		return -1;

	return 0;
}
#endif	/* #if defined(CONFIG_MATRIX) */

#if defined(CONFIG_DDR3)
static void ddramc_reg_config(struct ddramc_register *ddramc_config)
{
	ddramc_config->mdr = (AT91C_DDRC2_DBW_32_BITS |
			      AT91C_DDRC2_MD_DDR3_SDRAM);

	ddramc_config->cr = (AT91C_DDRC2_NC_DDR10_SDR9 |
			     AT91C_DDRC2_NR_14 |
			     AT91C_DDRC2_CAS_5 |
			     AT91C_DDRC2_DIS_DLL_DISABLED |
			     AT91C_DDRC2_WEAK_STRENGTH_RZQ7 |
			     AT91C_DDRC2_NB_BANKS_8 |
			     AT91C_DDRC2_DECOD_INTERLEAVED |
			     AT91C_DDRC2_UNAL_SUPPORTED);

	/*
	 * According to MT41K128M16 datasheet
	 * Maximum fresh period: 64ms, refresh count: 8k
	 */
#ifdef CONFIG_BUS_SPEED_166MHZ
	/* Refresh Timer is (64ms / 8k) * 166MHz = 1297(0x511) */
	ddramc_config->rtr = 0x511;

	/*
	 * According to the sama5d2 datasheet and the following values:
	 * T Sens = 0.75%/C, V Sens = 0.2%/mV, T driftrate = 1C/sec and V driftrate = 15 mV/s
	 * Warning: note that the values T driftrate and V driftrate are dependent on
	 * the application environment.
	 * ZQCS period is 1.5 / ((0.75 x 1) + (0.2 x 15)) = 0.4s
	 * If tref is 7.8us, we have: 400000 / 7.8 = 51282(0xC852)
	 * */
	ddramc_config->cal_mr4r = AT91C_DDRC2_COUNT_CAL(0xC852);

	/* DDR3 ZQCS */
	ddramc_config->tim_calr = AT91C_DDRC2_ZQCS(64);

	/* Assume timings for 8ns min clock period */
	ddramc_config->t0pr = (AT91C_DDRC2_TRAS_(6) |
			       AT91C_DDRC2_TRCD_(3) |
			       AT91C_DDRC2_TWR_(4) |
			       AT91C_DDRC2_TRC_(9) |
			       AT91C_DDRC2_TRP_(3) |
			       AT91C_DDRC2_TRRD_(4) |
			       AT91C_DDRC2_TWTR_(4) |
			       AT91C_DDRC2_TMRD_(4));

	ddramc_config->t1pr = (AT91C_DDRC2_TRFC_(27) |
			       AT91C_DDRC2_TXSNR_(29) |
			       AT91C_DDRC2_TXSRD_(0) |
			       AT91C_DDRC2_TXP_(3));

	ddramc_config->t2pr = (AT91C_DDRC2_TXARD_(0) |
			       AT91C_DDRC2_TXARDS_(0) |
			       AT91C_DDRC2_TRPA_(0) |
			       AT91C_DDRC2_TRTP_(4) |
			       AT91C_DDRC2_TFAW_(7));
#else
#error "No CLK setting defined"
#endif
}

static void ddramc_init(void)
{
	struct ddramc_register ddramc_reg;
	unsigned int reg;

	ddramc_reg_config(&ddramc_reg);

	pmc_sam9x5_enable_periph_clk(AT91C_ID_MPDDRC);
	pmc_enable_system_clock(AT91C_PMC_DDR);

	/* MPDDRC I/O Calibration Register */
	reg = readl(AT91C_BASE_MPDDRC + MPDDRC_IO_CALIBR);
	reg &= ~AT91C_MPDDRC_RDIV;
	reg |= AT91C_MPDDRC_RDIV_DDR2_RZQ_50;
	reg &= ~AT91C_MPDDRC_TZQIO;
	reg |= AT91C_MPDDRC_TZQIO_(100);
	writel(reg, (AT91C_BASE_MPDDRC + MPDDRC_IO_CALIBR));

	writel(AT91C_MPDDRC_RD_DATA_PATH_TWO_CYCLES,
			(AT91C_BASE_MPDDRC + MPDDRC_RD_DATA_PATH));

	ddr3_sdram_initialize(AT91C_BASE_MPDDRC, AT91C_BASE_DDRCS, &ddramc_reg);

	ddramc_dump_regs(AT91C_BASE_MPDDRC);
}
#endif

#ifdef CONFIG_HW_INIT
void hw_init(void)
{
	/* Disable watchdog */
	at91_disable_wdt();

	/*
	 * while coming from the ROM code, we run on PLLA @ 396 MHz / 132 MHz
	 * so we need to slow down and configure MCKR accordingly.
	 * This is why we have a special flavor of the switching function.
	 */

	/* Switch PCK/MCK clock source to the main clock */
	pmc_cfg_mck_down(BOARD_PRESCALER_MAIN_CLOCK);

	/* Configure PLLA */
	pmc_cfg_plla(PLLA_SETTINGS);

	/* Switch MCK clock source to PLLA */
	pmc_cfg_mck(BOARD_PRESCALER_PLLA);

	/* Enable external reset */
	writel(AT91C_RSTC_KEY_UNLOCK | AT91C_RSTC_URSTEN,
	       AT91C_BASE_RSTC + RSTC_RMR);

#if defined(CONFIG_MATRIX)
	/* Initialize the matrix */
	matrix_init();
#endif
	/* initialize the dbgu */
	initialize_dbgu();

	/* Init timer */
	timer_init();

#if defined(CONFIG_DDR3)
	/* Initialize MPDDR Controller */
	ddramc_init();
#endif
	/* Prepare L2 cache setup */
	l2cache_prepare();
}
#endif /* #ifdef CONFIG_HW_INIT */

#ifdef CONFIG_DATAFLASH
void at91_spi0_hw_init(void)
{
	const struct pio_desc spi_pins[] = {
		{"SPI0_SPCK",	AT91C_PIN_PA(14), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SPI0_MOSI",	AT91C_PIN_PA(15), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SPI0_MISO",	AT91C_PIN_PA(16), 0, PIO_DEFAULT, PIO_PERIPH_A},
		{"SPI0_NPCS",	CONFIG_SYS_SPI_PCS, 1, PIO_DEFAULT, PIO_OUTPUT},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};

	pio_configure(spi_pins);
	pmc_sam9x5_enable_periph_clk(CONFIG_SYS_ID_SPI);
}
#endif

#if defined(CONFIG_TWI0)
unsigned int at91_twi0_hw_init(void)
{
	unsigned int base_addr = AT91C_BASE_TWI0;

	const struct pio_desc twi_pins[] = {
		{"TWD0", AT91C_PIN_PD(21), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{"TWCK0", AT91C_PIN_PD(22), 0, PIO_DEFAULT, PIO_PERIPH_B},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};

	pio_configure(twi_pins);
	pmc_sam9x5_enable_periph_clk(AT91C_ID_TWI0);

	return base_addr;
}
#endif

#ifdef CONFIG_NANDFLASH
void nandflash_hw_init(void)
{
	const struct pio_desc nand_pins[] = {
		{"NANDOE", CONFIG_SYS_NAND_OE_PIN, 0, PIO_PULLUP, PIO_PERIPH_F},
		{"NANDWE", CONFIG_SYS_NAND_WE_PIN, 0, PIO_PULLUP, PIO_PERIPH_F},
		{"NANDALE", CONFIG_SYS_NAND_ALE_PIN, 0, PIO_PULLUP, PIO_PERIPH_F},
		{"NANDCLE", CONFIG_SYS_NAND_CLE_PIN, 0, PIO_PULLUP, PIO_PERIPH_F},
		{"NANDCS", CONFIG_SYS_NAND_ENABLE_PIN, 1, PIO_DEFAULT, PIO_OUTPUT},
		{"D0", AT91C_PIN_PA(0), 0, PIO_PULLUP, PIO_PERIPH_F},
		{"D1", AT91C_PIN_PA(1), 0, PIO_PULLUP, PIO_PERIPH_F},
		{"D2", AT91C_PIN_PA(2), 0, PIO_PULLUP, PIO_PERIPH_F},
		{"D3", AT91C_PIN_PA(3), 0, PIO_PULLUP, PIO_PERIPH_F},
		{"D4", AT91C_PIN_PA(4), 0, PIO_PULLUP, PIO_PERIPH_F},
		{"D5", AT91C_PIN_PA(5), 0, PIO_PULLUP, PIO_PERIPH_F},
		{"D6", AT91C_PIN_PA(6), 0, PIO_PULLUP, PIO_PERIPH_F},
		{"D7", AT91C_PIN_PA(7), 0, PIO_PULLUP, PIO_PERIPH_F},
		{(char *)0, 0, 0, PIO_DEFAULT, PIO_PERIPH_A},
	};

	pio_configure(nand_pins);
	pmc_sam9x5_enable_periph_clk(AT91C_ID_HSMC);

	/* EBI Configuration Register */
	writel((AT91C_EBICFG_DRIVE0_HIGH |
		AT91C_EBICFG_PULL0_NONE |
		AT91C_EBICFG_DRIVE1_HIGH |
		AT91C_EBICFG_PULL1_NONE), SFR_EBICFG + AT91C_BASE_SFR);

	/* Configure SMC CS3 for NAND/SmartMedia */
	writel(AT91C_SMC_SETUP_NWE(1) |
	       AT91C_SMC_SETUP_NCS_WR(1) |
	       AT91C_SMC_SETUP_NRD(1) |
	       AT91C_SMC_SETUP_NCS_RD(1), (ATMEL_BASE_SMC + SMC_SETUP3));

	writel(AT91C_SMC_PULSE_NWE(2) |
	       AT91C_SMC_PULSE_NCS_WR(3) |
	       AT91C_SMC_PULSE_NRD(2) |
	       AT91C_SMC_PULSE_NCS_RD(3), (ATMEL_BASE_SMC + SMC_PULSE3));

	writel(AT91C_SMC_CYCLE_NWE(5) |
	       AT91C_SMC_CYCLE_NRD(5), (ATMEL_BASE_SMC + SMC_CYCLE3));

	writel(AT91C_SMC_TIMINGS_TCLR(2) |
	       AT91C_SMC_TIMINGS_TADL(7) |
	       AT91C_SMC_TIMINGS_TAR(2) |
	       AT91C_SMC_TIMINGS_TRR(3) |
	       AT91C_SMC_TIMINGS_TWB(7) |
	       AT91C_SMC_TIMINGS_RBNSEL(2) |
	       AT91C_SMC_TIMINGS_NFSEL, (ATMEL_BASE_SMC + SMC_TIMINGS3));

	writel(AT91C_SMC_MODE_READMODE_NRD_CTRL |
	       AT91C_SMC_MODE_WRITEMODE_NWE_CTRL |
	       AT91C_SMC_MODE_DBW_8 |
	       AT91C_SMC_MODE_TDF_CYCLES(1), (ATMEL_BASE_SMC + SMC_MODE3));
}
#endif /* #ifdef CONFIG_NANDFLASH */

#if defined(CONFIG_TWI1)
unsigned int at91_twi1_hw_init(void)
{
	return 0;
}
#endif

#if defined(CONFIG_AUTOCONFIG_TWI_BUS)
void at91_board_config_twi_bus(void)
{
	act8865_twi_bus = 0;
}
#endif

#if defined(CONFIG_ACT8865_SET_VOLTAGE)
int at91_board_act8865_set_reg_voltage(void)
{
	unsigned char reg, value;
	int ret;

	/* Check ACT8865 I2C interface */
	if (act8865_check_i2c_disabled())
		return 0;

	/* Enable REG4 output 2.5V */
	reg = REG4_0;
	value = ACT8865_2V5;
	ret = act8865_set_reg_voltage(reg, value);
	if (ret) {
		dbg_loud("ACT8865: Failed to make REG4 output 2500mV\n");
		return -1;
	}

	/* Enable REG5 output 3.3V */
	reg = REG5_0;
	value = ACT8865_3V3;
	ret = act8865_set_reg_voltage(reg, value);
	if (ret) {
		dbg_loud("ACT8865: Failed to make REG5 output 3300mV\n");
		return -1;
	}

	/* Enable REG6 output 2.5V */
	reg = REG6_0;
	value = ACT8865_2V5;
	ret = act8865_set_reg_voltage(reg, value);
	if (ret) {
		dbg_loud("ACT8865: Failed to make REG6 output 2500mV\n");
		return -1;
	}

	/* Enable REG7 output 1.8V */
	reg = REG7_0;
	value = ACT8865_1V8;
	ret = act8865_set_reg_voltage(reg, value);
	if (ret) {
		dbg_loud("ACT8865: Failed to make REG7 output 1800mV\n");
		return -1;
	}

	return 0;
}
#endif