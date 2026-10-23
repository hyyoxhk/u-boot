# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 He Yong <hyyoxhk@163.com>

"""Create a GPT disk containing valid and malformed raw script images."""

import os
import struct
import zlib

import utils
from tests import fs_helper


CASES = ('legacy', 'fit', 'external', 'bad_crc',
         'too_large', 'wrong_name', 'empty', 'bad_header', 'partition_size',
         'bad_fit')


def script_image(ubman, case):
    """Build one script image, optionally corrupting its header or data."""
    workdir = ubman.config.build_dir
    script = os.path.join(workdir, 'raw-script.cmd')
    image = os.path.join(workdir, 'raw-script.scr')
    mkimage = os.path.join(ubman.config.build_dir, 'tools', 'mkimage')
    with open(script, 'w', encoding='utf-8') as outf:
        outf.write('echo raw-script-ok ${devtype} ${devnum} '
                   '${distro_bootpart}\n')
    if case in ('fit', 'external', 'bad_fit'):
        its = os.path.join(workdir, 'raw-script.its')
        with open(its, 'w', encoding='utf-8') as outf:
            outf.write('''/dts-v1/;
/ {
    description = "Raw script test";
    #address-cells = <1>;
    images {
        script {
            description = "Boot script";
            data = /incbin/("raw-script.cmd");
            type = "script";
            arch = "sandbox";
            compression = "none";
            hash { algo = "sha256"; };
        };
    };
    configurations {
        default = "conf";
        conf {
            script = "script";
        };
    };
};
''')
        args = [mkimage, '-f', its]
        if case == 'external':
            args.append('-E')
        utils.run_and_log(ubman, args + [image])
    else:
        utils.run_and_log(ubman, [mkimage, '-A', 'sandbox', '-T', 'script',
                                  '-C', 'none', '-d', script, image])
    with open(image, 'rb') as inf:
        data = bytearray(inf.read())
    if case == 'bad_crc':
        data[-1] ^= 1
    elif case == 'too_large':
        struct.pack_into('>I', data, 12, 2 * 1024 * 1024)
    elif case == 'empty':
        data = bytearray(512)
    if case == 'too_large':
        struct.pack_into('>I', data, 4, 0)
        struct.pack_into('>I', data, 4, zlib.crc32(data[:64]))
    return data


def setup_script_raw_image(ubman):
    """Create one disk for all cases, with 512 KiB non-bootable partitions."""
    spec = 'label: gpt\n'
    for part, case in enumerate(CASES, 1):
        start = part * 2048
        label = 'other' if case == 'wrong_name' else 'uboot_scr'
        spec += f'start={start}, size=1024, name="{label}"\n'
    disk, _ = fs_helper.setup_image(ubman, 12, None, img_size=16,
                                    part_spec=spec)
    with open(disk, 'r+b') as outf:
        for part, case in enumerate(CASES, 1):
            data = script_image(ubman, case)
            if case == 'bad_header':
                data[4] ^= 1
            elif case == 'partition_size':
                struct.pack_into('>I', data, 12, 512 * 1024)
            elif case == 'bad_fit':
                # Keep the header valid but corrupt the structure block.
                offset = struct.unpack_from('>I', data, 8)[0]
                struct.pack_into('>I', data, offset, 0xffffffff)
            if case == 'partition_size':
                struct.pack_into('>I', data, 4, 0)
                struct.pack_into('>I', data, 4, zlib.crc32(data[:64]))
            outf.seek(part * 1024 * 1024)
            outf.write(data)
