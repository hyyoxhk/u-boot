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

/* Known-size FITs must be structurally valid and contain all their data. */
static int check_fit_script_bounds(struct unit_test_state *uts)
{
	static const char script[] = "true";
	static const char * const external_props[] = {
		FIT_DATA_OFFSET_PROP, FIT_DATA_POSITION_PROP,
	};
	char fit[1024] __aligned(8), bad[1024] __aligned(8);
	const void *expected;
	char *data;
	uint len;
	int images, node, confs, conf, hash, size, i;

	ut_assertok(fdt_create_empty_tree(fit, sizeof(fit)));
	ut_assertok(fdt_setprop_string(fit, 0, FIT_DESC_PROP, "Script test"));
	ut_assertok(fdt_setprop_u32(fit, 0, FIT_TIMESTAMP_PROP, 0));
	images = fdt_add_subnode(fit, 0, "images");
	ut_assert(images >= 0);
	ut_assertok(fdt_setprop_string(fit, images, FIT_DEFAULT_PROP, "script"));
	node = fdt_add_subnode(fit, images, "script");
	ut_assert(node >= 0);
	ut_assertok(fdt_setprop_string(fit, node, FIT_TYPE_PROP, "script"));
	ut_assertok(fdt_setprop(fit, node, FIT_DATA_PROP, script, sizeof(script)));
	hash = fdt_add_subnode(fit, node, "hash");
	ut_assert(hash >= 0);
	ut_assertok(fdt_setprop_string(fit, hash, FIT_ALGO_PROP, "crc32"));
	ut_assertok(fdt_setprop_u32(fit, hash, FIT_VALUE_PROP,
				    crc32(0, (const u8 *)script, sizeof(script))));
	confs = fdt_add_subnode(fit, 0, "configurations");
	ut_assert(confs >= 0);
	ut_assertok(fdt_setprop_string(fit, confs, FIT_DEFAULT_PROP, "conf"));
	conf = fdt_add_subnode(fit, confs, "conf");
	ut_assert(conf >= 0);
	ut_assertok(fdt_setprop_string(fit, conf, FIT_SCRIPT_PROP, "script"));
	ut_assertok(fdt_pack(fit));
	size = fdt_totalsize(fit);
	node = fdt_path_offset(fit, "/images/script");
	ut_assert(node >= 0);
	expected = fdt_getprop(fit, node, FIT_DATA_PROP, NULL);
	ut_assertnonnull(expected);

	/* Exercise configuration lookup, direct image lookup and source. */
	ut_assertok(image_locate_script(fit, size, NULL, NULL, &data, &len));
	ut_asserteq(sizeof(script), len);
	ut_asserteq_ptr(expected, data);
	ut_assertok(image_locate_script(fit, size, "script", NULL, &data, &len));
	ut_assertok(cmd_source_script(map_to_sysmem(fit), NULL, NULL));

	/* Structural validation follows fit_check_format() configuration. */
	if (CONFIG_IS_ENABLED(FIT_FULL_CHECK)) {
		ut_asserteq(1, image_locate_script(fit, sizeof(struct fdt_header) - 1,
						   NULL, NULL, &data, &len));
		ut_asserteq(1, image_locate_script(fit, sizeof(struct fdt_header),
						   NULL, NULL, &data, &len));
		ut_asserteq(1, image_locate_script(fit, size - 1, NULL, NULL,
						   &data, &len));

		memcpy(bad, fit, size);
		fdt_set_totalsize(bad, sizeof(bad) + 1);
		ut_asserteq(1, image_locate_script(bad, sizeof(bad), NULL, NULL,
						   &data, &len));

		/* A valid header does not imply a valid structure block. */
		memcpy(bad, fit, size);
		*(fdt32_t *)(bad + fdt_off_dt_struct(bad)) = cpu_to_fdt32(0xffffffff);
		ut_assertok(fdt_check_header(bad));
		ut_asserteq(1, image_locate_script(bad, size, NULL, NULL, &data, &len));
	}

	for (i = 0; i < ARRAY_SIZE(external_props); i++) {
		ut_assertok(fdt_open_into(fit, bad, sizeof(bad)));
		node = fdt_path_offset(bad, "/images/script");
		ut_assert(node >= 0);
		ut_assertok(fdt_delprop(bad, node, FIT_DATA_PROP));
		ut_assertok(fdt_setprop_u32(bad, node, external_props[i],
					    0xfffffffc));
		ut_assertok(fdt_setprop_u32(bad, node, FIT_DATA_SIZE_PROP,
					    sizeof(script)));
		ut_asserteq(1, image_locate_script(bad, sizeof(bad), NULL,
						   NULL, &data, &len));

		/* Reject external data even in an unselected image. */
		ut_assertok(fdt_open_into(fit, bad, sizeof(bad)));
		images = fdt_path_offset(bad, FIT_IMAGES_PATH);
		ut_assert(images >= 0);
		node = fdt_add_subnode(bad, images, "external");
		ut_assert(node >= 0);
		ut_assertok(fdt_setprop_u32(bad, node, external_props[i],
					    0xfffffffc));
		ut_asserteq(1, image_locate_script(bad, sizeof(bad), NULL,
						   NULL, &data, &len));
	}

	return 0;
}

static int image_fit_script_bounds(struct unit_test_state *uts)
{
	int ret, verify;

	if (!IS_ENABLED(CONFIG_FIT))
		return -EAGAIN;

	for (verify = 0; verify < 2; verify++) {
		ut_assertok(env_set("verify", verify ? "yes" : "no"));
		ret = check_fit_script_bounds(uts);
		ut_assertok(env_set("verify", NULL));
		ut_assertok(ret);
	}

	return 0;
}
BOOTSTD_TEST(image_fit_script_bounds, 0);
