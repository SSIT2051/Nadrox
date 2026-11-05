#!/bin/bash
set -e

echo "ARM Translation Engine Test Builder"
echo "===================================="
echo ""

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$TEST_DIR"

# Check for ARM cross-compiler
if command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
    echo "Found arm-linux-gnueabihf-gcc"
    ARM_CC="arm-linux-gnueabihf-gcc"
    ARM_CXX="arm-linux-gnueabihf-g++"
    HAS_CROSS=1
elif command -v arm-none-linux-gnueabihf-gcc >/dev/null 2>&1; then
    echo "Found arm-none-linux-gnueabihf-gcc"
    ARM_CC="arm-none-linux-gnueabihf-gcc"
    ARM_CXX="arm-none-linux-gnueabihf-g++"
    HAS_CROSS=1
else
    echo "No ARM cross-compiler found."
    echo "Attempting to use qemu-arm for verification..."
    HAS_CROSS=0
fi

echo ""
echo "Building test binaries..."
echo ""

if [ $HAS_CROSS -eq 1 ]; then
    # Build static ARM hello world
    echo "Building static ARM hello world..."
    $ARM_CC -static -O2 -o hello_arm.elf hello_arm.c
    echo "  -> hello_arm.elf"
    
    # Build from assembly if needed
    # $ARM_CC -static -o hello_arm_asm.elf hello_arm.s
    
    # Build shared library
    echo "Building ARM shared library..."
    $ARM_CC -shared -fPIC -O2 -o libtest_arm.so test_lib.c
    echo "  -> libtest_arm.so"
    
    # Try to build with debug symbols
    $ARM_CC -shared -fPIC -g -o libtest_arm_debug.so test_lib.c 2>/dev/null || true
    
    echo ""
    echo "Built ARM binaries:"
    file hello_arm.elf libtest_arm.so 2>&1 | head -5
else
    echo "Cannot build ARM binaries without cross-compiler."
    echo "Please install: gcc-arm-linux-gnueabihf or use qemu-user-static"
    echo ""
    echo "On Debian/Ubuntu: apt install gcc-arm-linux-gnueabihf libc6-dev-armhf-cross"
    echo "On Arch: pacman -S arm-linux-gnueabihf-gcc"
    echo "On Fedora: dnf install arm-linux-gnueabihf-gcc"
fi

echo ""
echo "Checking qemu-arm..."
if command -v qemu-arm >/dev/null 2>&1; then
    echo "Found qemu-arm"
    if [ -f hello_arm.elf ] && [ $HAS_CROSS -eq 1 ]; then
        echo "Testing with qemu-arm-static or qemu-arm..."
        qemu-arm -L /usr/arm-linux-gnueabihf hello_arm.elf 2>&1 | head -3 || \
        qemu-arm-static -L /usr/arm-linux-gnueabihf hello_arm.elf 2>&1 | head -3 || \
        echo "  (qemu test may need different setup)"
    fi
else
    echo "qemu-arm not found"
fi

echo ""
echo "Test files created in: $TEST_DIR"
ls -lh *.c *.s *.sh 2>&1 | head -15
