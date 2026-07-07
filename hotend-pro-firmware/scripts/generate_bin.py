"""
Post-build script: copies the .elf output to Robin_nano_v3.1.bin
in the project root for direct SD-card flashing.
"""

import os
import shutil
import subprocess

Import("env")

def after_build(source, target, env):
    elf_path = str(target[0])
    bin_path = os.path.join(env.subst("$PROJECT_DIR"), "Robin_nano_v3.1.bin")

    # Convert ELF → binary using arm-none-eabi-objcopy
    objcopy = env.subst("$OBJCOPY")
    cmd = [objcopy, "-O", "binary", elf_path, bin_path]
    try:
        subprocess.check_call(cmd)
        print(f"\n>>> Firmware binary: {bin_path}")
        size = os.path.getsize(bin_path)
        print(f">>> Size: {size:,} bytes ({size / 1024:.1f} KB)\n")
    except Exception as e:
        print(f">>> WARNING: Could not generate .bin: {e}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", after_build)
