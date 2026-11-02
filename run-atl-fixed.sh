#!/bin/bash
# ATL Runner - sets up all library paths for the built ATL
ATL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ATL_DIR/build"

export PATH="/tmp/kilo/bin:/opt/android-sdk/cmake/3.22.1/bin:/opt/android-sdk/build-tools/34.0.0:/usr/lib/jvm/java-17-openjdk/bin:$PATH"
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
export PKG_CONFIG_PATH="/tmp/kilo/pkgconfig:$BUILD_DIR/lib/pkgconfig:$PKG_CONFIG_PATH"

# Library paths
export LD_LIBRARY_PATH="/tmp/kilo/wk_extracted/usr/lib:/tmp/kilo/lp_extracted/usr/lib:/tmp/kilo/vk_extracted/usr/lib:$BUILD_DIR/lib:$BUILD_DIR/lib/art:$BUILD_DIR/bionic_build:$BUILD_DIR/atl_build:$LD_LIBRARY_PATH"

export RUN_FROM_BUILDDIR=1

# Resolve APK path
args=()
for arg in "$@"; do
	if [[ -f "$arg" ]]; then
		args+=("$(realpath "$arg")")
	else
		args+=("$arg")
	fi
done

cd "$BUILD_DIR/atl_build"
exec ./android-translation-layer --sdk-int=28 "${args[@]}"
