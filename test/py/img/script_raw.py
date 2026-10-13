# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 He Yong <hyyoxhk@163.com>

"""Create a GPT disk containing valid and malformed raw script images."""

import os
import struct
import zlib

import utils


CASES = ('legacy', 'fit', 'external', 'bad_crc', 'no_terminator',
         'too_large', 'wrong_name', 'empty', 'bad_header', 'partition_size',
         'script_length', 'bad_fit')


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
    elif case == 'no_terminator':
        data[64:] = b'\xff' * (len(data) - 64)
    elif case == 'too_large':
        struct.pack_into('>I', data, 12, 2 * 1024 * 1024)
    elif case == 'empty':
        data = bytearray(512)
    if case in ('no_terminator', 'too_large'):
        if case == 'no_terminator':
            struct.pack_into('>I', data, 24, zlib.crc32(data[64:]))
        struct.pack_into('>I', data, 4, 0)
        struct.pack_into('>I', data, 4, zlib.crc32(data[:64]))
    return data


def setup_script_raw_image(ubman):
    """Create one disk for all cases, with 512 KiB non-bootable partitions."""
    disk = os.path.join(ubman.config.source_dir, 'mmc12.img')
    with open(disk, 'wb') as outf:
        outf.truncate(16 * 1024 * 1024)
    args = ['sgdisk', '--clear']
    for part, case in enumerate(CASES, 1):
        start = part * 2048
        label = 'other' if case == 'wrong_name' else 'uboot_scr'
        args += ['-n', f'{part}:{start}:{start + 1023}',
                 '-c', f'{part}:{label}']
    utils.run_and_log(ubman, args + [disk])
    with open(disk, 'r+b') as outf:
        for part, case in enumerate(CASES, 1):
            data = script_image(ubman, case)
            if case == 'bad_header':
                data[4] ^= 1
            elif case == 'partition_size':
                struct.pack_into('>I', data, 12, 512 * 1024)
            elif case == 'script_length':
                struct.pack_into('>I', data, 64, len(data))
                struct.pack_into('>I', data, 24, zlib.crc32(data[64:]))
            elif case == 'bad_fit':
                # Keep the header valid but corrupt the structure block.
                offset = struct.unpack_from('>I', data, 8)[0]
                struct.pack_into('>I', data, offset, 0xffffffff)
            if case in ('partition_size', 'script_length'):
                struct.pack_into('>I', data, 4, 0)
                struct.pack_into('>I', data, 4, zlib.crc32(data[:64]))
            outf.seek(part * 1024 * 1024)
            outf.write(data)
