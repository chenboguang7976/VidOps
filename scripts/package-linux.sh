#!/usr/bin/env bash
# Builds VidOps-<version>-x86_64.AppImage with Qt, yt-dlp and ffmpeg bundled.
#
#   scripts/package-linux.sh BUILD_DIR OUT_DIR VERSION
# Needs: curl, patchelf, and qmake for the Qt used to build (QMAKE=... if not on PATH).
set -euo pipefail

build=$(cd "${1:?build dir}" && pwd)
mkdir -p "${2:?output dir}"
out=$(cd "$2" && pwd)
version=${3:-dev}
root=$(cd "$(dirname "$0")/.." && pwd)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
appdir="$work/AppDir"

DESTDIR="$appdir" cmake --install "$build" --prefix /usr
"$root/scripts/fetch-tools.sh" "$appdir/usr/bin/tools" linux

get() { curl -fL --retry 4 --retry-delay 3 -o "$work/$1" "$2"; chmod +x "$work/$1"; }
get linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
get linuxdeploy-plugin-qt https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

export APPIMAGE_EXTRACT_AND_RUN=1  # no FUSE on CI runners
export OUTPUT="$out/VidOps-$version-x86_64.AppImage"
export PATH="$work:$PATH"
(cd "$work" && linuxdeploy --appdir "$appdir" \
    --executable "$appdir/usr/bin/VidOps" \
    --desktop-file "$root/resources/vidops.desktop" \
    --icon-file "$root/resources/vidops.png" \
    --plugin qt --output appimage)
echo "created $OUTPUT"
