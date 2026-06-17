#
# PlatformIO post-build script
#
# After a successful build, copies the generated firmware .bin into dist/
# named  display7_<VERSION>_b<BUILD>.bin , where VERSION is read from the
#   #define VERSION "x.y.z"
# line in src/main.cpp, and BUILD is a persistent counter that increments on
# every successful build (stored in scripts/build_count.txt).
#
import os
import re
import shutil

Import("env")  # noqa: F821 (injected by PlatformIO/SCons)

# Output filename pattern. {ver} is the version parsed from main.cpp and
# {build} is the zero-padded incrementing build count.
NAME_TEMPLATE = "display7_{ver}_b{build:04d}.bin"

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


# Run after the firmware .bin is produced.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
