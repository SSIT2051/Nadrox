#!/bin/bash
# Build NADROX distribution package
# Usage: ./build_package.sh [version]

set -euo pipefail

VERSION="${1:-0.1.0}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
DIST_DIR="${PROJECT_ROOT}/dist"
PACKAGE_NAME="nadrox-${VERSION}"

echo "=== Building NADROX ${VERSION} ==="

# Clean previous builds
rm -rf "${BUILD_DIR}" "${DIST_DIR}"
mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

# Configure
echo "Configuring CMake..."
cd "${PROJECT_ROOT}"
cmake -B "${BUILD_DIR}" -S . \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_Java_COMPILER=/usr/lib/jvm/java-21-openjdk/bin/javac \
  -DBUILD_FEX_EMU=OFF \
  -DBUILD_BOX86_FROM_SOURCE=OFF

# Build
echo "Building..."
cmake --build "${BUILD_DIR}" -j$(nproc)

# Create package directory
PACKAGE_DIR="${DIST_DIR}/${PACKAGE_NAME}"
rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}"

# Install to package directory
echo "Installing to package directory..."
DESTDIR="${PACKAGE_DIR}" cmake --install "${BUILD_DIR}"

# Create source tarball (for PKGBUILD)
echo "Creating source tarball..."
cd "${DIST_DIR}"
tar -czf "${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}"

# Create AppImage using linuxdeploy (optional)
if command -v linuxdeploy &> /dev/null; then
  echo "Creating AppImage..."
  linuxdeploy --appdir "${PACKAGE_DIR}/AppDir" --output appimage
fi

echo ""
echo "=== Build Complete ==="
echo "Package: ${DIST_DIR}/${PACKAGE_NAME}.tar.gz"
echo "Install directory: ${PACKAGE_DIR}"
echo ""
echo "To install on Arch/CachyOS:"
echo "  cd ${DIST_DIR}"
echo "  tar -xzf ${PACKAGE_NAME}.tar.gz"
echo "  cd ${PACKAGE_NAME}"
echo "  makepkg -si  # (requires PKGBUILD in packaging/)"
echo ""
echo "Or install directly:"
echo "  sudo cp -r ${PACKAGE_DIR}/usr/* /usr/"
echo "  sudo update-desktop-database"
echo "  sudo update-mime-database /usr/share/mime"
echo "  sudo gtk-update-icon-cache -f /usr/share/icons/hicolor"