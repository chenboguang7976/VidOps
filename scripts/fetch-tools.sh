#!/usr/bin/env bash
# Downloads standalone yt-dlp, ffmpeg and ffprobe into DEST so they can be
# bundled with VidOps (VidOps looks next to its executable and in ./tools).
#
#   scripts/fetch-tools.sh DEST [linux|macos]
set -euo pipefail

dest=${1:?usage: fetch-tools.sh DEST [linux|macos]}
platform=${2:-}
if [[ -z "$platform" ]]; then
  case "$(uname -s)" in
    Linux) platform=linux ;;
    Darwin) platform=macos ;;
    *) echo "unsupported platform, use scripts/fetch-tools.ps1 on Windows" >&2; exit 1 ;;
  esac
fi

mkdir -p "$dest"
dest=$(cd "$dest" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fetch() { curl -fL --retry 4 --retry-delay 3 -o "$2" "$1"; }

case "$platform" in
  linux)
    fetch https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux "$dest/yt-dlp"
    # Shared build: ffmpeg and ffprobe share one set of libraries (about half
    # the size of two static binaries). Needs patchelf to point RPATH at ./lib.
    fetch https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-linux64-gpl-shared.tar.xz "$work/ffmpeg.tar.xz"
    tar -xJf "$work/ffmpeg.tar.xz" -C "$work"
    src=$(echo "$work"/ffmpeg-*/)
    mkdir -p "$dest/lib"
    cp "$src/bin/ffmpeg" "$src/bin/ffprobe" "$dest/"
    # Only the SONAME files (libavcodec.so.NN), dereferenced.
    for lib in "$src"/lib/*.so.*; do
      name=$(basename "$lib")
      [[ "$name" =~ \.so\.[0-9]+$ ]] && cp -L "$lib" "$dest/lib/$name"
    done
    patchelf --set-rpath '$ORIGIN/lib' "$dest/ffmpeg" "$dest/ffprobe"
    ;;
  macos)
    fetch https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos "$dest/yt-dlp"
    # Static builds from https://ffmpeg.martin-riedl.de, merged into universal binaries.
    for tool in ffmpeg ffprobe; do
      slices=()
      for arch in arm64 amd64; do
        fetch "https://ffmpeg.martin-riedl.de/redirect/latest/macos/$arch/release/$tool.zip" "$work/$tool-$arch.zip"
        mkdir -p "$work/$arch"
        unzip -o -q "$work/$tool-$arch.zip" -d "$work/$arch"
        slices+=("$work/$arch/$tool")
      done
      lipo -create "${slices[@]}" -output "$dest/$tool"
    done
    ;;
  *)
    echo "unknown platform: $platform" >&2; exit 1 ;;
esac

chmod +x "$dest/yt-dlp" "$dest/ffmpeg" "$dest/ffprobe"
"$dest/ffmpeg" -hide_banner -version | head -n1
echo "tools ready in $dest"
