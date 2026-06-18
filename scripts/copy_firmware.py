#
# PlatformIO post-build script
#
# After a successful build, copies the generated firmware .bin into dist/
# named  display7_<VERSION>_b<BUILD>.bin , where VERSION is read from the
#   #define VERSION "x.y.z"
# line in src/main.cpp, and BUILD is a persistent counter that increments on
# every successful build (stored in scripts/build_count.txt).
#
# It also produces a single-file "factory" image
#   factory_display7_<VERSION>_b<BUILD>.bin
# that merges the bootloader, partition table, boot_app0 and the application
# into one .bin. That file can be flashed at offset 0x0 directly with the
# official Espressif tool, e.g.:
#   esptool --chip esp32s3 write_flash 0x0 factory_display7_<VERSION>_b<BUILD>.bin
# or via Espressif's "ESP Flash Download Tool" (single bin @ 0x0).
#
import os
import re
import shutil
import subprocess

Import("env")  # noqa: F821 (injected by PlatformIO/SCons)

# Output filename pattern. {ver} is the version parsed from main.cpp and
# {build} is the zero-padded incrementing build count.
NAME_TEMPLATE = "display7_{ver}_b{build:04d}.bin"

# Single-file flashable image (bootloader + partitions + app), flashed @ 0x0.
FACTORY_NAME_TEMPLATE = "factory_display7_{ver}_b{build:04d}.bin"

# File that persists the monotonic build counter across builds.
BUILD_COUNT_FILE = os.path.join(env["PROJECT_DIR"], "scripts", "build_count.txt")  # noqa: F821


def next_build_count():
    """Read, increment, and persist the build counter. Returns the new value."""
    count = 0
    try:
        with open(BUILD_COUNT_FILE, "r", encoding="utf-8") as f:
            count = int(f.read().strip() or "0")
    except (OSError, ValueError):
        count = 0

    count += 1

    try:
        with open(BUILD_COUNT_FILE, "w", encoding="utf-8") as f:
            f.write(str(count))
    except OSError as e:
        print("copy_firmware: cannot write build counter: %s" % e)

    return count


def read_version():
    main_cpp = os.path.join(env["PROJECT_DIR"], "src", "main.cpp")
    try:
        with open(main_cpp, "r", encoding="utf-8") as f:
            content = f.read()
    except OSError as e:
        print("copy_firmware: cannot read main.cpp: %s" % e)
        return "unknown"

    m = re.search(r'#define\s+VERSION\s+"([^"]+)"', content)
    if not m:
        print("copy_firmware: VERSION define not found in main.cpp")
        return "unknown"
    return m.group(1)


def flash_images(app_bin):
    """Return the ordered list of (offset, image_path) to merge into one bin.

    Uses the offsets/images PlatformIO already prepared for this build
    (bootloader, partition table, boot_app0), then appends the application at
    its configured offset. Offsets are kept as the strings PlatformIO uses
    (e.g. "0x8000") so esptool gets exactly what the normal flash uses.
    """
    images = []
    # FLASH_EXTRA_IMAGES is a list of (offset, path) for bootloader,
    # partitions and boot_app0, in the same form the uploader uses.
    for offset, path in env.get("FLASH_EXTRA_IMAGES", []):
        images.append((str(offset), env.subst(path)))

    app_offset = env.subst("$ESP32_APP_OFFSET") or "0x10000"
    images.append((app_offset, app_bin))

    # Flash in ascending offset order (esptool merge_bin expects this).
    images.sort(key=lambda pair: int(str(pair[0]), 0))
    return images


def build_factory_bin(app_bin, dst_bin):
    """Merge bootloader + partitions + boot_app0 + app into one bin @ 0x0."""
    esptool_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")
    esptool_py = os.path.join(esptool_dir, "esptool.py")

    chip = env.BoardConfig().get("build.mcu", "esp32s3")
    flash_mode = env.subst("$BOARD_FLASH_MODE") or "qio"
    flash_freq = env.BoardConfig().get("build.f_flash", "80000000L")
    flash_freq = str(flash_freq).replace("000000L", "m").replace("L", "")
    flash_size = env.BoardConfig().get("upload.flash_size", "16MB")

    cmd = [
        env.subst("$PYTHONEXE"), esptool_py,
        "--chip", chip,
        "merge_bin",
        "-o", dst_bin,
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
    ]
    for offset, path in flash_images(app_bin):
        cmd += [offset, path]

    try:
        subprocess.run(cmd, check=True)
        print("copy_firmware: built factory image -> %s" % dst_bin)
    except (subprocess.CalledProcessError, OSError) as e:
        print("copy_firmware: failed to build factory image: %s" % e)


def after_build(source, target, env):
    version = read_version()

    src_bin = os.path.join(env.subst("$BUILD_DIR"), env.subst("${PROGNAME}.bin"))
    if not os.path.isfile(src_bin):
        print("copy_firmware: firmware not found at %s" % src_bin)
        return

    build = next_build_count()

    dist_dir = os.path.join(env["PROJECT_DIR"], "dist")
    os.makedirs(dist_dir, exist_ok=True)

    dst_bin = os.path.join(dist_dir, NAME_TEMPLATE.format(ver=version, build=build))
    shutil.copyfile(src_bin, dst_bin)
    print("copy_firmware: copied firmware -> %s (build #%d)" % (dst_bin, build))

    factory_bin = os.path.join(
        dist_dir, FACTORY_NAME_TEMPLATE.format(ver=version, build=build)
    )
    build_factory_bin(src_bin, factory_bin)


# Run after the firmware .bin is produced.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
