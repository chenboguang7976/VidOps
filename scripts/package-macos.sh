#!/usr/bin/env bash
# Builds VidOps-<version>-macos-universal.dmg with Qt, yt-dlp and ffmpeg bundled.
#
#   scripts/package-macos.sh BUILD_DIR OUT_DIR VERSION
# Needs macdeployqt (from the Qt used to build) on PATH.
set -euo pipefail

build=$(cd "${1:?build dir}" && pwd)
mkdir -p "${2:?output dir}"
out=$(cd "$2" && pwd)
version=${3:-dev}
root=$(cd "$(dirname "$0")/.." && pwd)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
app="$work/VidOps.app"
cp -R "$build/VidOps.app" "$app"

macdeployqt "$app"
# VidOps looks for the tools next to its executable.
"$root/scripts/fetch-tools.sh" "$app/Contents/MacOS" macos

# Ad-hoc signature: required for arm64 binaries; not notarized.
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

stage="$work/dmg"
mkdir -p "$stage"
mv "$app" "$stage/"
ln -s /Applications "$stage/Applications"
dmg="$out/VidOps-$version-macos-universal.dmg"
hdiutil create -volname "VidOps" -srcfolder "$stage" -ov -format UDZO "$dmg"
echo "created $dmg"
