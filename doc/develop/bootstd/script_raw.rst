.. SPDX-License-Identifier: GPL-2.0+

Raw Script Bootmeth
==================

The ``script_raw`` bootmeth reads a script image from the start of a named
partition on a block device, without a filesystem. Enable
``CONFIG_BOOTMETH_SCRIPT`` and ``CONFIG_BOOTMETH_SCRIPT_RAW`` to build it.
The partition name is selected by ``CONFIG_BOOTMETH_SCRIPT_RAW_PARTITION``
and defaults to ``uboot_scr``. The partition need not be marked bootable.

The method reads the first block to determine the image size, then reads the
complete image. The image must fit within the partition and the limit set by
``CONFIG_BOOTMETH_SCRIPT_RAW_MAX_SIZE`` (1 MiB by default). It supports legacy
U-Boot script images and FIT scripts with embedded data. FIT external data is
not supported. Plain text scripts must first be wrapped with ``mkimage``, for
example::

    mkimage -A arm64 -T script -C none -n 'Boot script' -d boot.cmd boot.scr

Write ``boot.scr`` directly at the start of the ``uboot_scr`` partition when
creating the disk image. There is no filename lookup on the target partition.
Image validation is performed before the bootflow is marked ready, using the
same verification policy as ``source`` (including the ``verify`` environment
variable).

To scan only this bootmeth on MMC device 0::

    bootmeth order script_raw
    bootflow scan -lb mmc0

To use the configured bootmeth order for normal automatic boot, set
``CONFIG_BOOTCOMMAND="bootflow scan -lb"``. Existing saved environments may
override this default; check ``printenv bootcmd bootmeths boot_targets``.
Restore the default bootmeth order with ``bootmeth order`` if necessary.

The script is executed from the bootflow's allocated buffer, so ``scriptaddr``
is not required. Before execution, the method sets ``devtype``, ``devnum`` and
``distro_bootpart`` as described in :doc:`script`, with an empty ``prefix``.
The script itself is responsible for selecting and booting an A/B slot.

The compatible string is ``u-boot,script-raw``. On boards which explicitly
declare bootmeth devices in their device tree, add a child node with this
compatible under the ``bootstd`` node. Otherwise standard boot automatically
binds the enabled bootmeth drivers.
