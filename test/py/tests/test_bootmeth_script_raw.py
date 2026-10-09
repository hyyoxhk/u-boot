# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 He Yong <hyyoxhk@163.com>

"""Test script images stored directly in a GPT partition."""

import os
import struct
import zlib

import pytest
import utils


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('bootmeth_script_raw')
@pytest.mark.buildconfigspec('legacy_image_format')
@pytest.mark.buildconfigspec('fit')
@pytest.mark.buildconfigspec('cmd_host')
@pytest.mark.buildconfigspec('cmd_bind')
@pytest.mark.buildconfigspec('cmd_bootflow')
@pytest.mark.parametrize('case', [
    'legacy', 'fit', 'external', 'bad_crc', 'no_terminator', 'too_large',
    'wrong_name', 'empty',
])
def test_bootmeth_script_raw(ubman, case):
    """Scan and execute valid images; reject malformed or unrelated images."""
    ubman.restart_uboot()
    workdir = ubman.config.result_dir
    script = os.path.join(workdir, 'raw-script.cmd')
    image = os.path.join(workdir, 'raw-script.scr')
    disk = os.path.join(workdir, 'raw-script.img')
    mkimage = os.path.join(ubman.config.build_dir, 'tools', 'mkimage')
    with open(script, 'w', encoding='utf-8') as outf:
        outf.write('echo raw-script-ok ${devtype} ${devnum} '
                   '${distro_bootpart}\n')
    if case in ('fit', 'external'):
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
    with open(disk, 'wb') as outf:
        outf.truncate(4 * 1024 * 1024)
    label = 'other' if case == 'wrong_name' else 'uboot_scr'
    utils.run_and_log(ubman, ['sgdisk', '-n', '1:2048:4095',
                              '-c', f'1:{label}', disk])
    with open(disk, 'r+b') as outf:
        outf.seek(1024 * 1024)
        outf.write(data)
    try:
        ubman.run_command(f'host bind 0 {disk}')
        ubman.run_command('bind bootstd 0 bootmeth_script_raw')
        ubman.run_command('bootmeth order bootmeth_script_raw')
        ubman.run_command('setenv verify yes')
        output = ubman.run_command('bootflow scan -lb host0')
        if case in ('legacy', 'fit'):
            assert 'raw-script-ok host 0 1' in output
        else:
            assert 'raw-script-ok' not in output
            assert 'No more bootdevs' in output
    finally:
        ubman.restart_uboot()
