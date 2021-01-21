// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 hey <hyyoxhk@163.com>
 *
 * Configuation settings for the iTop-4412 (EXYNOS4412) board.
 */

#include <common.h>
#include <linux/delay.h>
#include <log.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/power.h>
#include <asm/arch/clock.h>
#include <asm/arch/gpio.h>
#include <asm/gpio.h>
#include <asm/arch/cpu.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/s5m8767.h>
#include <errno.h>
#include <mmc.h>
#include <usb.h>
#include <usb/dwc2_udc.h>
#include "setup.h"

DECLARE_GLOBAL_DATA_PTR;

/* iTop-4412 board types */
enum {
	ITOP_TYPE_SCP,
	ITOP_TYPE_POP,
	ITOP_TYPE_REV,
};

#ifdef CONFIG_BOARD_TYPES
/* HW revision with core board */
static u32 board_rev = 1;

u32 get_board_rev(void)
{
	return board_rev;
}

static u32 get_cpu_type(void)
{
	u32 pro_id = readl(EXYNOS4_PRO_ID);
	u32 package = pro_id >> 8;

	return package & 0xf;
}

void set_board_type(void)
{
	switch (get_cpu_type()) {
	case 0x0:
		gd->board_type = ITOP_TYPE_SCP;
		break;
	case 0x2:
		gd->board_type = ITOP_TYPE_POP;
		break;
	default:
		gd->board_type = ITOP_TYPE_REV;
		break;
	}
}

void set_board_revision(void)
{
	/*
	 * Revision already set by set_board_type() because it can be
	 * executed early.
	 */
}

const char *get_board_type(void)
{
	const char *board_type[] = {"SCP", "POP"};

	return board_type[gd->board_type];
}
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
static void setup_board_info(void)
{
	const char *fdt_compat;
	int fdt_compat_len;
	char info[64];

	snprintf(info, ARRAY_SIZE(info), "%u.%u", (s5p_cpu_rev & 0xf0) >> 4, s5p_cpu_rev & 0xf);
	env_set("soc_rev", info);

	snprintf(info, ARRAY_SIZE(info), "%x", s5p_cpu_id);
	env_set("soc_id", info);

	snprintf(info, ARRAY_SIZE(info), "%x", get_board_rev());
	env_set("board_rev", info);

	fdt_compat = fdt_getprop(gd->fdt_blob, 0, "compatible", &fdt_compat_len);
	if (fdt_compat && fdt_compat_len) {
		env_set("board_name", fdt_compat + 7);

		snprintf(info, ARRAY_SIZE(info), "%s%x-%s.dtb", CONFIG_SYS_SOC,
			s5p_cpu_id, fdt_compat + 7);
		env_set("fdtfile", info);
	}
}

static void setup_boot_mode(void)
{
	u32 bootmode = get_boot_mode();

	switch (bootmode) {
	case BOOT_MODE_SD:
		env_set("boot_device", "mmc");
		env_set("boot_instance", "0");
	case BOOT_MODE_EMMC_SD:
		env_set("boot_device", "mmc");
		env_set("boot_instance", "1");
		break;
	default:
		pr_debug("unexpected boot mode = %x\n", bootmode);
		break;
	}
}

void set_board_info(void)
{
	setup_board_info();
	setup_boot_mode();
}
#endif

#ifdef CONFIG_SET_DFU_ALT_INFO
char *get_dfu_alt_system(char *interface, char *devstr)
{
	return env_get("dfu_alt_system");
}

char *get_dfu_alt_boot(char *interface, char *devstr)
{
	struct mmc *mmc;
	char *alt_boot;
	int dev_num;

	dev_num = simple_strtoul(devstr, NULL, 10);

	mmc = find_mmc_device(dev_num);
	if (!mmc)
		return NULL;

	if (mmc_init(mmc))
		return NULL;

	alt_boot = IS_SD(mmc) ? CONFIG_DFU_ALT_BOOT_SD :
				CONFIG_DFU_ALT_BOOT_EMMC;

	return alt_boot;
}
#endif

static void board_clock_init(void)
{
	struct exynos4x12_clock *clk =
			(struct exynos4x12_clock *)samsung_get_base_clock();

	/* Set PDIV, MDIV, and SDIV values (Refer to (A, M, E, V)
	 * Change other PLL control values
	 */
	writel(CLK_DIV_CPU0_VAL, &clk->div_cpu0);
	writel(CLK_DIV_CPU1_VAL, &clk->div_cpu1);
	writel(CLK_DIV_DMC0_VAL, &clk->div_dmc0);
	writel(CLK_DIV_DMC1_VAL, &clk->div_dmc1);
	writel(CLK_DIV_TOP_VAL, &clk->div_top);
	writel(CLK_DIV_LEFTBUS_VAL, &clk->div_leftbus);
	writel(CLK_DIV_RIGHTBUS_VAL, &clk->div_rightbus);
	writel(CLK_DIV_PERIL0_VAL, &clk->div_peril0);
	writel(CLK_DIV_FSYS0_VAL, &clk->div_fsys0);
	writel(CLK_DIV_FSYS1_VAL, &clk->div_fsys1);
	writel(CLK_DIV_FSYS2_VAL, &clk->div_fsys2);
	writel(CLK_DIV_FSYS3_VAL, &clk->div_fsys3);

	/* Set K, AFC, MRR, MFR values if necessary
	 * (Refer to (A, M, E, V)PLL_CON1 SFRs)
	 * Turn on a PLL (Refer to (A, M, E, V) PLL_CON0 SFRs)
	 */
	writel(APLL_CON1_VAL, &clk->apll_con1);
	writel(APLL_CON0_VAL, &clk->apll_con0);
	writel(MPLL_CON1_VAL, &clk->mpll_con1);
	writel(MPLL_CON0_VAL, &clk->mpll_con0);
	writel(EPLL_CON2_VAL, &clk->epll_con2);
	writel(EPLL_CON0_VAL, &clk->epll_con0);
	writel(VPLL_CON2_VAL, &clk->vpll_con2);
	writel(VPLL_CON0_VAL, &clk->vpll_con0);

	/* Wait until the PLL is locked */
	writel(APLL_LOCKTIME, &clk->apll_lock);
	writel(MPLL_LOCKTIME, &clk->mpll_lock);
	writel(EPLL_LOCKTIME, &clk->epll_lock);
	writel(VPLL_LOCKTIME, &clk->vpll_lock);

	/* Select the PLL output clock instead of input reference clock,
	 * after PLL output clock is stabilized.
	 * (Refer to CLK_SRC_CPU SFR for APLL and MPLL,
	 * CLK_SRC_TOP0 for EPLL and VPLL)
	 * Once a PLL is turned on, do not turn it off.
	 */
	writel(CLK_SRC_CPU_VAL, &clk->src_cpu);
	writel(CLK_SRC_DMC_VAL, &clk->src_dmc);
	writel(CLK_SRC_TOP0_VAL, &clk->src_top0);
	writel(CLK_SRC_TOP1_VAL, &clk->src_top1);
	writel(CLK_SRC_LEFTBUS_VAL, &clk->src_leftbus);
	writel(CLK_SRC_RIGHTBUS_VAL, &clk->src_rightbus);
	writel(CLK_SRC_PERIL0_VAL, &clk->src_peril0);
	writel(CLK_SRC_FSYS_VAL, &clk->src_fsys);
}

int exynos_early_init_f(void)
{
	board_clock_init();

	return 0;
}

static void board_gpio_init(void)
{
	/* eMMC Reset Pin */
	gpio_request(EXYNOS4X12_GPIO_K02, "eMMC Reset");
	gpio_direction_output(EXYNOS4X12_GPIO_K02, 1);

	/* LED */
	gpio_request(EXYNOS4X12_GPIO_L20, "LED2");
	gpio_direction_output(EXYNOS4X12_GPIO_L20, 1);

#ifdef CONFIG_CMD_USB
	/* USB3503A Reference Intn */
	gpio_request(EXYNOS4X12_GPIO_M23, "USB3503A Intn");
	gpio_direction_output(EXYNOS4X12_GPIO_M23, 0);

	/* USB3503A Connect */
	gpio_request(EXYNOS4X12_GPIO_M33, "USB3503A Connect");
	gpio_direction_output(EXYNOS4X12_GPIO_M33, 0);

	/* USB3503A Reset */
	gpio_request(EXYNOS4X12_GPIO_M24, "USB3503A Reset");
	gpio_direction_output(EXYNOS4X12_GPIO_M24, 0);

	/* Reset */
	gpio_direction_output(EXYNOS4X12_GPIO_M24, 1);

	/* From usb3503 linux driver ? */
	mdelay(4);

	/* Connect */
	gpio_direction_output(EXYNOS4X12_GPIO_M33, 1);
#endif
}

int exynos_init(void)
{
	board_gpio_init();

	return 0;
}

int exynos_power_init(void)
{
	int ret;
	struct udevice *dev;

	ret = pmic_get("s5m8767-pmic", &dev);
	/* TODO(sjg@chromium.org): Use driver model to access clock */
	if (!ret)
		s5m8767_enable_32khz_cp(dev);

	if (ret == -ENODEV)
		return 0;

	ret = regulators_enable_boot_on(false);
	if (ret)
		return ret;

	return 0;
}

const char *env_ext4_get_dev_part(void)
{
	static char *const dev_part[] = {"0:auto", "1:auto"};
	u32 bootmode = get_boot_mode();

	if (bootmode == BOOT_MODE_SD) {
		return dev_part[0];
	}

	return dev_part[1];
}

enum env_location env_get_location(enum env_operation op, int prio)
{
	u32 bootmode = get_boot_mode();

	if (prio)
		return ENVL_UNKNOWN;

	switch (bootmode) {
	case BOOT_MODE_SD:
	case BOOT_MODE_EMMC_SD:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_MMC))
			return ENVL_MMC;
		else if (CONFIG_IS_ENABLED(ENV_IS_IN_EXT4))
			return ENVL_EXT4;
		else
			return ENVL_NOWHERE;
	default:
		return ENVL_NOWHERE;
	}
}

const char *env_ext4_get_intf(void)
{
	u32 bootmode = get_boot_mode();

	switch (bootmode) {
	case BOOT_MODE_SD:
	case BOOT_MODE_EMMC_SD:
		return "mmc";
	default:
		return "";
	}
}
