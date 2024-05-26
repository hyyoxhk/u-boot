/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2020 hey <hyyoxhk@163.com>
 *
 * Configuation settings for the iTop-4412 (EXYNOS4412) board.
 */

#ifndef __CONFIG_ITOP_H
#define __CONFIG_ITOP_H

#include <configs/exynos4-common.h>

/*#define CONFIG_SYS_L2CACHE_OFF*/
#ifndef CONFIG_SYS_L2CACHE_OFF
#define CFG_SYS_PL310_BASE	0x10502000
#endif

#define CONFIG_MACH_TYPE	5115

#define CFG_SYS_SDRAM_BASE	0x40000000
#define SDRAM_BANK_SIZE		(256 << 20)	/* 256 MB */
#define PHYS_SDRAM_1		CFG_SYS_SDRAM_BASE

#include <linux/sizes.h>

/*
 * For booting Linux, use the first 512 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CFG_SYS_BOOTMAPSZ		SZ_512M

#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(MMC, mmc, 1)

#define ITOP_EXTRA \
	"env_check=if env info -p -d -q; then env save; fi\0"

#define ITOP_BOARD_EXTRA_ENV \
	"console=ttySAC1\0" \

#define ITOP_BOOTCMD "bootcmd_itop=" \
	"echo \"Boot over ${boot_device}${boot_instance}!\"; " \
	"run env_check; " \
	"if test ${boot_device} = mmc; then; " \
		"env set boot_targets \"mmc${boot_instance}\"; " \
	"fi;" \
	"if test ${boot_targets} = mmc0; then; " \
		"env set ramroot /dev/ram0; " \
		"env set bootroot ${ramroot}; " \
	"fi;" \
	"if test ${boot_targets} = mmc1; then; " \
		"env set mmcroot /dev/mmcblk${boot_instance}p${boot_part}; " \
		"env set bootroot ${mmcroot}; " \
	"fi;" \
	"run distro_bootcmd;\0" \

#include <config_distro_bootcmd.h>

#define __KERNEL_ADDR_R     __stringify(0x42000000)
#define __FDT_ADDR_R        __stringify(0x44000000)
#define __SCRIPT_ADDR_R     __stringify(0x44100000)
#define __PXEFILE_ADDR_R    __stringify(0x44200000)
#define __FDTOVERLAY_ADDR_R __stringify(0x44300000)
#define __RAMDISK_ADDR_R    __stringify(0x44400000)

#define ITOP_MEM_LAYOUT \
	"kernel_addr_r=" __KERNEL_ADDR_R "\0" \
	"fdt_addr_r=" __FDT_ADDR_R "\0" \
	"scriptaddr=" __SCRIPT_ADDR_R "\0" \
	"pxefile_addr_r=" __PXEFILE_ADDR_R "\0" \
	"fdtoverlay_addr_r=" __FDTOVERLAY_ADDR_R "\0" \
	"ramdisk_addr_r=" __RAMDISK_ADDR_R "\0"

/*
 * Bootable media layout:
 * dev:    SD   eMMC(part boot)
 * BL1      1    0
 * BL2     31   30
 * UBOOT   63   62
 * TZSW  2111 2110
 */
#define CFG_EXTRA_ENV_SETTINGS \
	ITOP_MEM_LAYOUT \
	ITOP_BOOTCMD \
	BOOTENV \
	ITOP_EXTRA \
	ITOP_BOARD_EXTRA_ENV

#endif	/* __CONFIG_H */
