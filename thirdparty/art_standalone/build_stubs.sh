#!/usr/bin/env bash
# Build minimal stub art_standalone libraries, binaries and JARs for linking.
# Invoked from CMakeLists.txt via: bash -e build_stubs.sh
set -euo pipefail

OUT_DIR="${1:-/tmp/art_stubs_out}"

# Accept LIBDIR env (e.g. lib or lib64), otherwise compute lib vs lib64.
if [ -n "${LIBDIR:-}" ]; then
    LIBDIR_NAME="${LIBDIR}"
else
    case "$(uname -m)" in
        x86_64|aarch64|ppc64le|s390x|mips64*|riscv64) LIBDIR_NAME="lib64" ;;
        *) LIBDIR_NAME="lib" ;;
    esac
fi

LIB_DIR="${OUT_DIR}/${LIBDIR_NAME}"
BIN_DIR="${OUT_DIR}/bin"
FRAMEWORK_DIR="${OUT_DIR}/framework"

mkdir -p "${LIB_DIR}" "${BIN_DIR}" "${FRAMEWORK_DIR}"

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"

# Create stub libraries with correct SONAME
for lib in libandroidfw libart libart-compiler libart-dexlayout libartbase \
           libartpalette libbacktrace libbase libcutils libdexfile liblog \
           libnativebridge libprofile libsigchain libunwind libutils \
           libziparchive libjavacore libnativehelper libopenjdk libopenjdkjvm \
           libwolfssljni; do
    echo 'int dummy(void) { return 0; }' > /tmp/dummy.c
    # Provide real stub symbols for libs that need them
    if [ "${lib}" = "libandroidfw" ]; then
        cc -shared -fPIC -Wl,-soname,"${lib}.so" -o "${LIB_DIR}/${lib}.so" \
            "${SRC_DIR}/stubs_androidfw.c"
    elif [ "${lib}" = "libutils" ]; then
        c++ -shared -fPIC -Wl,-soname,"${lib}.so" -o "${LIB_DIR}/${lib}.so" \
            "${SRC_DIR}/stubs_utils.cpp"
    elif [ "${lib}" = "liblog" ]; then
        cc -shared -fPIC -Wl,-soname,"${lib}.so" -o "${LIB_DIR}/${lib}.so" \
            "${SRC_DIR}/stubs_log.c"
    else
        cc -shared -fPIC -Wl,-soname,"${lib}.so" -o "${LIB_DIR}/${lib}.so" /tmp/dummy.c
    fi
done

# Create stub binaries
for bin in dex2oat dx dalvikvm dalvikvm64 adbd; do
    echo 'int main(void) { return 0; }' > /tmp/dummy.c
    cc -o "${BIN_DIR}/${bin}" /tmp/dummy.c
done

# Create stub JARs
STAGING_DIR="$(mktemp -d)"
for jar in apachehttp-hostdex apache-xml-hostdex bouncycastle-hostdex \
             core-junit-hostdex core-libart-hostdex core-oj-hostdex \
             hamcrest-hostdex junit-runner-hostdex okhttp-hostdex \
             wolfssljni-hostdex; do
    jar cf "${FRAMEWORK_DIR}/${jar}.jar" -C "${STAGING_DIR}" .
done
rm -rf "${STAGING_DIR}"

# Create stub dx.jar
STAGING_DIR="$(mktemp -d)"
jar cf "${FRAMEWORK_DIR}/dx.jar" -C "${STAGING_DIR}" .
rm -rf "${STAGING_DIR}"

# Create stub core classes JARs in the common obj directory
# (core-all, core-junit, junit-runner) - referenced by CMakeLists.txt
COMMON_OBJ_DIR="${OUT_DIR%/*}/common/obj/JAVA_LIBRARIES"
mkdir -p "${COMMON_OBJ_DIR}"
STAGING_DIR="$(mktemp -d)"
for dir in core-all-hostdex_intermediates core-junit-hostdex_intermediates \
            junit-runner-hostdex_intermediates; do
    mkdir -p "${COMMON_OBJ_DIR}/${dir}"
    jar cf "${COMMON_OBJ_DIR}/${dir}/classes.jar" -C "${STAGING_DIR}" .
done
rm -rf "${STAGING_DIR}"