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

#include <config_distro_bootcmd.h>

/*
 * Bootable media layout:
 * dev:    SD   eMMC(part boot)
 * BL1      1    0
 * BL2     31   30
 * UBOOT   63   62
 * TZSW  2111 2110
 * ENV   2560 2560(part user)
 *
 * MBR Primary partiions:
 * Num Name   Size  Offset
 * 1.  BOOT:  100MiB 2MiB
 * 2.  ROOT:  -
 */
#define CFG_EXTRA_ENV_SETTINGS \
	"loadbootscript=load mmc ${mmcbootdev}:${mmcbootpart} ${scriptaddr} " \
		"boot.scr\0" \
	"loadkernel=load mmc ${mmcbootdev}:${mmcbootpart} ${kernel_addr_r} " \
		"${kernelname}\0" \
	"loadinitrd=load mmc ${mmcbootdev}:${mmcbootpart} ${ramdisk_addr_r} " \
		"${initrdname}\0" \
	"loaddtb=load mmc ${mmcbootdev}:${mmcbootpart} ${fdt_addr_r} " \
		"${fdtfile}\0" \
	"check_ramdisk=" \
		"if run loadinitrd; then " \
			"setenv initrd_addr ${ramdisk_addr_r};" \
		"else " \
			"setenv initrd_addr -;" \
		"fi;\0" \
	"check_dtb=" \
		"if run loaddtb; then " \
			"setenv fdt_addr ${fdt_addr_r};" \
		"else " \
			"setenv fdt_addr;" \
		"fi;\0" \
	"kernel_args=" \
		"setenv bootargs root=/dev/mmcblk${mmcrootdev}p${mmcrootpart}" \
		" rootwait ${console} ${opts}\0" \
	"boot_script=" \
		"run loadbootscript;" \
		"source ${scriptaddr}\0" \
	"boot_fit=" \
		"setenv kernelname Image.itb;" \
		"run loadkernel;" \
		"run kernel_args;" \
		"bootm ${kernel_addr_r}#${board_name}\0" \
	"boot_uimg=" \
		"setenv kernelname uImage;" \
		"run check_dtb;" \
		"run check_ramdisk;" \
		"run loadkernel;" \
		"run kernel_args;" \
		"bootm ${kernel_addr_r} ${initrd_addr} ${fdt_addr};\0" \
	"boot_zimg=" \
		"setenv kernelname zImage;" \
		"run check_dtb;" \
		"run check_ramdisk;" \
		"run loadkernel;" \
		"run kernel_args;" \
		"bootz ${kernel_addr_r} ${initrd_addr} ${fdt_addr};\0" \
	"autoboot=" \
		"if test -e mmc ${mmcbootdev} boot.scr; then; " \
			"run boot_script; " \
		"elif test -e mmc ${mmcbootdev} Image.itb; then; " \
			"run boot_fit;" \
		"elif test -e mmc ${mmcbootdev} zImage; then; " \
			"run boot_zimg;" \
		"elif test -e mmc ${mmcbootdev} uImage; then; " \
			"run boot_uimg;" \
		"fi;\0" \
	"console=console=ttySAC1,115200n8\0" \
	"mmcbootdev=0\0" \
	"mmcbootpart=1\0" \
	"mmcrootdev=0\0" \
	"mmcrootpart=2\0" \
	"consoleon=set console console=ttySAC1,115200n8; save; reset\0" \
	"consoleoff=set console console=ram; save; reset\0" \
	"initrdname=uInitrd\0" \
	"ramdisk_addr_r=0x44400000\0" \
	"fdtoverlay_addr_r=0x44300000\0" \
	"pxefile_addr_r=0x44200000\0" \
	"scriptaddr=0x44100000\0" \
	"fdt_addr_r=0x44000000\0" \
	"kernel_addr_r=0x42000000\0" \
	BOOTENV

#endif	/* __CONFIG_H */
