# NADROX Packaging Guide

## Quick Start (Arch/CachyOS)

```bash
# 1. Build and create package
./scripts/build_package.sh 0.1.0

# 2. Install via makepkg (requires PKGBUILD in packaging/)
cd dist/nadrox-0.1.0
makepkg -si
```

## What Gets Installed

```
/usr/bin/nadrox                    # Launcher (entry point)
/usr/bin/nadrox.bin                # Main binary
/usr/lib/nadrox/                   # Native libraries
  libandroid.so
  libtranslation_layer_main.so
  libwolfssl.so
  libart*.so
  libbionic_*.so
/usr/lib/java/dex/android_translation_layer/
  api-impl.jar
  gstub.jar
  test_runner.jar
  framework-res.apk
  natives/                         # JNI libraries
/usr/share/applications/nadrox.desktop
/usr/share/mime/packages/nadrox.xml
/usr/share/icons/hicolor/*/apps/nadrox.svg
/usr/share/libalpm/hooks/nadrox.hook
```

## Post-Install (Automatic via pacman hooks)

The `.hook` file triggers:
- `update-desktop-database` - Registers `.desktop` entry
- `update-mime-database` - Registers `.apk` MIME type
- `gtk-update-icon-cache` - Updates icon cache

## Usage After Install

```bash
# Double-click any .apk file in file manager
# Or from terminal:
nadrox game.apk

# With options:
nadrox --no-fullscreen game.apk
nadrox --disable-pc-input game.apk
```

## Creating a System Package

### Arch Linux (makepkg)

```bash
# The PKGBUILD in packaging/ uses the source tarball
# Build in clean chroot (recommended):
extra-x86_64-build
```

### Debian/Ubuntu

Create `debian/` directory with:
- `control` - Package metadata
- `rules` - Build rules  
- `postinst` - Post-install (update-desktop-database, etc.)
- `postrm` - Cleanup on removal

### Fedora/RHEL

Create `.spec` file with:
- `%files` section listing all installed files
- `%post` / `%postun` scripts for desktop/mime/icon cache

## Icon Theme

The icons use a scalable SVG (`nadrox.svg`) that works at all sizes.
For specific sizes, render PNGs from the SVG:

```bash
for size in 16 32 48 64 128 256; do
  inkscape -w $size -h $size packaging/icons/hicolor/scalable/apps/nadrox.svg \
    -o packaging/icons/hicolor/${size}x${size}/apps/nadrox.png
done
```

## Desktop Entry Details

The `.desktop` file registers:
- `MimeType=application/vnd.android.package-archive` - Handles `.apk` files
- `Exec=nadrox %f` - Opens clicked APK
- `StartupNotify=true` - Shows loading animation
- `Categories=Game;Emulator` - Appears in Game menu

## Testing Without Install

```bash
# Run from build directory
cd build
./android-translation-layer /path/to/game.apk

# With environment variables
NADROX_FULLSCREEN=1 NADROX_PC_INPUT=1 ./android-translation-layer game.apk
```