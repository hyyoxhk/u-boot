// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for vbe-simple bootmeth. All start with 'vbe_simple'
 *
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <image.h>
#include <mapmem.h>
#include <u-boot/crc.h>
#include <test/ut.h>
#include <linux/libfdt.h>
#include "bootstd_common.h"

/* Test that the default configuration breaks best-match ties */
static int test_fit_conf_find_compat(struct unit_test_state *uts)
{
	char fdt[256], fit[1024];
	int confs, images, node;
	int ret;

	/* control devicetree with a two-entry compatible list */
	ut_assertok(fdt_create_empty_tree(fdt, sizeof(fdt)));
	ut_assertok(fdt_appendprop_string(fdt, 0, "compatible",
					  "test,board-a"));
	ut_assertok(fdt_appendprop_string(fdt, 0, "compatible",
					  "test,fallback"));

	/* FIT with two configurations matching the same compatible */
	ut_assertok(fdt_create_empty_tree(fit, sizeof(fit)));
	images = fdt_add_subnode(fit, 0, "images");
	ut_assert(images >= 0);
	confs = fdt_add_subnode(fit, 0, "configurations");
	ut_assert(confs >= 0);
	ut_assertok(fdt_setprop_string(fit, confs, FIT_DEFAULT_PROP, "conf-2"));
	/*
	 * fdt_add_subnode() inserts before existing subnodes: create conf-2
	 * first so that conf-1 ends up listed first, like an .its compiled
	 * with the configurations in that order
	 */
	node = fdt_add_subnode(fit, confs, "conf-2");
	ut_assert(node >= 0);
	ut_assertok(fdt_setprop_string(fit, node, "compatible",
				       "test,board-a"));
	node = fdt_add_subnode(fit, confs, "conf-1");
	ut_assert(node >= 0);
	ut_assertok(fdt_setprop_string(fit, node, "compatible",
				       "test,board-a"));
	confs = fdt_path_offset(fit, "/configurations");
	node = fdt_first_subnode(fit, confs);
	ut_asserteq_str("conf-1", fdt_get_name(fit, node, NULL));

	/* on a tie, the default configuration wins */
	ret = fit_conf_find_compat(fit, fdt);
	ut_assert(ret > 0);
	ut_asserteq_str("conf-2", fdt_get_name(fit, ret, NULL));

	/* without a default, the first listed configuration wins */
	confs = fdt_path_offset(fit, "/configurations");
	ut_assertok(fdt_delprop(fit, confs, FIT_DEFAULT_PROP));
	confs = fdt_path_offset(fit, "/configurations");
	ut_assertnull((void *)fdt_getprop(fit, confs, FIT_DEFAULT_PROP, NULL));
	ret = fit_conf_find_compat(fit, fdt);
	ut_assert(ret > 0);
	ut_asserteq_str("conf-1", fdt_get_name(fit, ret, NULL));

	/* a strictly better match still beats the default */
	confs = fdt_path_offset(fit, "/configurations");
	ut_assertok(fdt_setprop_string(fit, confs, FIT_DEFAULT_PROP, "conf-2"));
	confs = fdt_path_offset(fit, "/configurations");
	node = fdt_subnode_offset(fit, confs, "conf-2");
	ut_assertok(fdt_setprop_string(fit, node, "compatible",
				       "test,fallback"));
	ret = fit_conf_find_compat(fit, fdt);
	ut_assert(ret > 0);
	ut_asserteq_str("conf-1", fdt_get_name(fit, ret, NULL));

	return 0;
}
BOOTSTD_TEST(test_fit_conf_find_compat, 0);

/* Test of image phase */
static int test_image_phase(struct unit_test_state *uts)
{
	int val;

	ut_asserteq_str("U-Boot phase", genimg_get_phase_name(IH_PHASE_U_BOOT));
	ut_asserteq_str("SPL Phase", genimg_get_phase_name(IH_PHASE_SPL));
	ut_asserteq_str("any", genimg_get_phase_name(IH_PHASE_NONE));
	ut_asserteq_str("Unknown Phase", genimg_get_phase_name(-1));

	ut_asserteq(IH_PHASE_U_BOOT, genimg_get_phase_id("u-boot"));
	ut_asserteq(IH_PHASE_SPL, genimg_get_phase_id("spl"));
	ut_asserteq(IH_PHASE_NONE, genimg_get_phase_id("none"));
	ut_asserteq(-1, genimg_get_phase_id("fred"));

	val = image_ph(IH_PHASE_SPL, IH_TYPE_FIRMWARE);
	ut_asserteq(IH_PHASE_SPL, image_ph_phase(val));
	ut_asserteq(IH_TYPE_FIRMWARE, image_ph_type(val));

	return 0;
}
BOOTSTD_TEST(test_image_phase, 0);

/* Recompute checksums after changing the legacy length table or payload. */
static void script_checksums(struct legacy_img_hdr *hdr)
{
	image_set_dcrc(hdr, crc32(0, (void *)image_get_data(hdr),
				  image_get_data_size(hdr)));
	image_set_hcrc(hdr, 0);
	image_set_hcrc(hdr, crc32(0, (void *)hdr, sizeof(*hdr)));
}

/* Bound both known buffers and source's unknown-size images by their header. */
static int image_legacy_script_bounds(struct unit_test_state *uts)
{
	struct {
		struct legacy_img_hdr hdr;
		u32 lengths[2];
		char script[4];
	} image;
	struct legacy_img_hdr *hdr = &image.hdr;
	char *data;
	uint len;
	int size, verify;

	if (!IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT))
		return -EAGAIN;
	for (verify = 0; verify < 2; verify++) {
		ut_assertok(env_set("verify", verify ? "yes" : "no"));
		memset(&image, 0, sizeof(image));
		image_set_magic(hdr, IH_MAGIC);
		image_set_type(hdr, IH_TYPE_SCRIPT);
		image_set_size(hdr, sizeof(image) - sizeof(*hdr));
		image.lengths[0] = cpu_to_uimage(sizeof(image.script));
		memcpy(image.script, "true", sizeof(image.script));
		script_checksums(hdr);
		ut_assertok(image_locate_script(&image, sizeof(image), NULL,
						NULL, &data, &len));
		ut_asserteq(sizeof(image.script), len);
		ut_asserteq_ptr(image.script, data);
		ut_assertok(cmd_source_script(map_to_sysmem(&image), NULL, NULL));

		/* A known size must bound reads before header/data CRC checks. */
		for (size = 1; size < sizeof(image); size++)
			ut_asserteq(1, image_locate_script(&image, size, NULL,
							   NULL, &data, &len));

		/* Missing terminator: do not walk into the script or beyond it. */
		memset(image.lengths, 0xff, sizeof(image) - sizeof(*hdr));
		script_checksums(hdr);
		ut_asserteq(1, image_locate_script(&image, sizeof(image), NULL,
						   NULL, &data, &len));
		ut_asserteq(1, image_locate_script(&image, 0, NULL, NULL,
						   &data, &len));
		ut_asserteq(CMD_RET_FAILURE,
			    cmd_source_script(map_to_sysmem(&image), NULL, NULL));

		/* A terminated table cannot name a script beyond the payload. */
		image.lengths[0] = cpu_to_uimage(sizeof(image.script) + 1);
		image.lengths[1] = 0;
		script_checksums(hdr);
		ut_asserteq(1, image_locate_script(&image, 0, NULL, NULL,
						   &data, &len));

		/* Reject a payload too short even for the length and terminator. */
		image_set_size(hdr, sizeof(u32));
		script_checksums(hdr);
		ut_asserteq(1, image_locate_script(&image, 0, NULL, NULL,
						   &data, &len));
	}

	return 0;
}

BOOTSTD_TEST(image_legacy_script_bounds, UTF_DM | UTF_SCAN_FDT);
