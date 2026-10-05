#!/usr/bin/env bash
# Downloads standalone yt-dlp, ffmpeg, ffprobe and deno (the JavaScript runtime
# yt-dlp needs for YouTube) into DEST so they can be
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
deno_url() { echo "https://github.com/denoland/deno/releases/latest/download/deno-$1.zip"; }

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
    fetch "$(deno_url x86_64-unknown-linux-gnu)" "$work/deno.zip"
    unzip -o -q "$work/deno.zip" -d "$dest"
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
    for arch in aarch64 x86_64; do
      fetch "$(deno_url $arch-apple-darwin)" "$work/deno-$arch.zip"
      mkdir -p "$work/deno-$arch"
      unzip -o -q "$work/deno-$arch.zip" -d "$work/deno-$arch"
    done
    lipo -create "$work/deno-aarch64/deno" "$work/deno-x86_64/deno" -output "$dest/deno"
    ;;
  *)
    echo "unknown platform: $platform" >&2; exit 1 ;;
esac

chmod +x "$dest/yt-dlp" "$dest/ffmpeg" "$dest/ffprobe" "$dest/deno"
"$dest/ffmpeg" -hide_banner -version | head -n1
"$dest/deno" --version | head -n1
echo "tools ready in $dest"
