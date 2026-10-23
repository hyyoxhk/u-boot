// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for scripts stored in a raw partition
 *
 * Copyright 2026 He Yong <hyyoxhk@163.com>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <blk.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <dm.h>
#include <image.h>
#include <malloc.h>
#include <mapmem.h>
#include <part.h>
#include <asm/cache.h>

static int script_raw_check(struct udevice *dev, struct bootflow_iter *iter)
{
	return bootflow_iter_check_blk(iter);
}

static int script_raw_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	struct legacy_img_hdr *hdr;
	struct disk_partition info;
	struct blk_desc *desc;
	lbaint_t count;
	uint size, len;
	char *data;
	void *buf;
	int ret;

	if (!bflow->blk || !bflow->part)
		return log_msg_ret("part", -ENOENT);
	desc = dev_get_uclass_plat(bflow->blk);
	ret = part_get_info(desc, bflow->part, &info);
	if (ret)
		return log_msg_ret("part", ret);
	if (strcmp(info.name, CONFIG_BOOTMETH_SCRIPT_RAW_PARTITION))
		return log_msg_ret("name", -ENOENT);
	bflow->state = BOOTFLOWST_PART;

	if (!info.size || desc->blksz < sizeof(*hdr) ||
	    desc->blksz > CONFIG_BOOTMETH_SCRIPT_RAW_MAX_SIZE)
		return log_msg_ret("block", -EINVAL);
	buf = memalign(ARCH_DMA_MINALIGN, desc->blksz);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	if (blk_dread(desc, info.start, 1, buf) != 1) {
		ret = log_msg_ret("header-read", -EIO);
		goto out;
	}

	hdr = buf;
	if (IS_ENABLED(CONFIG_LEGACY_IMAGE_FORMAT) &&
	    genimg_get_format(buf) == IMAGE_FORMAT_LEGACY) {
		if (!image_check_hcrc(hdr)) {
			ret = log_msg_ret("header", -EINVAL);
			goto out;
		}
		size = image_get_data_size(hdr);
		if (size > CONFIG_BOOTMETH_SCRIPT_RAW_MAX_SIZE - sizeof(*hdr)) {
			ret = log_msg_ret("legacy-size", -E2BIG);
			goto out;
		}
		size += sizeof(*hdr);
	} else if (IS_ENABLED(CONFIG_FIT) && !fdt_check_header(buf)) {
		size = fdt_totalsize(buf);
		if (size < sizeof(struct fdt_header)) {
			ret = log_msg_ret("fit-size", -EINVAL);
			goto out;
		}
	} else {
		ret = log_msg_ret("format", -ENOEXEC);
		goto out;
	}
	if (size > CONFIG_BOOTMETH_SCRIPT_RAW_MAX_SIZE) {
		ret = log_msg_ret("size", -E2BIG);
		goto out;
	}
	count = DIV_ROUND_UP(size, desc->blksz);
	if (count > info.size) {
		ret = log_msg_ret("partition-size", -E2BIG);
		goto out;
	}
	free(buf);
	buf = memalign(ARCH_DMA_MINALIGN, count * desc->blksz + 1);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	if (blk_dread(desc, info.start, count, buf) != count) {
		ret = log_msg_ret("image-read", -EIO);
		goto out;
	}
	((char *)buf)[size] = '\0';
	ret = image_locate_script(buf, size, NULL, NULL, &data, &len);
	if (ret) {
		ret = log_msg_ret("script", -EINVAL);
		goto out;
	}

	bflow->fname = strdup(CONFIG_BOOTMETH_SCRIPT_RAW_PARTITION);
	bflow->subdir = strdup("");
	if (!bflow->fname || !bflow->subdir) {
		ret = log_msg_ret("name-alloc", -ENOMEM);
		goto out;
	}
	if (!bootflow_img_add(bflow, bflow->fname, (enum bootflow_img_t)IH_TYPE_SCRIPT,
			      map_to_sysmem(buf), size)) {
		ret = log_msg_ret("image-alloc", -ENOMEM);
		goto out;
	}
	bflow->buf = buf;
	bflow->size = size;
	bflow->state = BOOTFLOWST_READY;

	return 0;
out:
	free(buf);
	return ret;
}

static int script_raw_bootmeth_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = "Script boot from a raw partition";
	plat->flags = BOOTMETHF_ANY_PART;

	return 0;
}

static struct bootmeth_ops script_raw_bootmeth_ops = {
	.check		= script_raw_check,
	.read_bootflow	= script_raw_read_bootflow,
	.boot		= bootmeth_script_boot,
};

static const struct udevice_id script_raw_bootmeth_ids[] = {
	{ .compatible = "u-boot,script-raw" },
	{ }
};

U_BOOT_DRIVER(bootmeth_2script_raw) = {
	.name		= "bootmeth_script_raw",
	.id		= UCLASS_BOOTMETH,
	.of_match	= script_raw_bootmeth_ids,
	.ops		= &script_raw_bootmeth_ops,
	.bind		= script_raw_bootmeth_bind,
};
