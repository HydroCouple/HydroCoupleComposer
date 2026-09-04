#!/usr/bin/env bash
# Regenerates every raster icon artifact from the SVG master at
# resources/branding/hydrocouplecomposer.svg:
#
#   resources/HydroCoupleComposer.icns       (full Retina membership)
#   resources/HydroCoupleComposer.ico        (16..256)
#   resources/images/hydrocouplecomposer.png (512; window icon + Doxygen logo)
#
# Requires rsvg-convert and ImageMagick everywhere, iconutil on macOS (the
# .icns is skipped elsewhere - regenerate it on a Mac before release).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SVG="$ROOT/resources/branding/hydrocouplecomposer.svg"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

render() { rsvg-convert -w "$1" -h "$1" "$SVG" -o "$2"; }

if command -v iconutil >/dev/null; then
    ICONSET="$WORK/HydroCoupleComposer.iconset"
    mkdir "$ICONSET"
    render 16   "$ICONSET/icon_16x16.png"
    render 32   "$ICONSET/icon_16x16@2x.png"
    render 32   "$ICONSET/icon_32x32.png"
    render 64   "$ICONSET/icon_32x32@2x.png"
    render 128  "$ICONSET/icon_128x128.png"
    render 256  "$ICONSET/icon_128x128@2x.png"
    render 256  "$ICONSET/icon_256x256.png"
    render 512  "$ICONSET/icon_256x256@2x.png"
    render 512  "$ICONSET/icon_512x512.png"
    render 1024 "$ICONSET/icon_512x512@2x.png"
    iconutil -c icns "$ICONSET" -o "$ROOT/resources/HydroCoupleComposer.icns"
    echo "wrote resources/HydroCoupleComposer.icns"
else
    echo "iconutil not found - skipping the .icns (regenerate on macOS)" >&2
fi

for size in 16 24 32 48 64 128 256; do
    render "$size" "$WORK/ico_$size.png"
done
magick "$WORK"/ico_16.png "$WORK"/ico_24.png "$WORK"/ico_32.png \
       "$WORK"/ico_48.png "$WORK"/ico_64.png "$WORK"/ico_128.png \
       "$WORK"/ico_256.png "$ROOT/resources/HydroCoupleComposer.ico"
echo "wrote resources/HydroCoupleComposer.ico"

render 512 "$ROOT/resources/images/hydrocouplecomposer.png"
echo "wrote resources/images/hydrocouplecomposer.png"
