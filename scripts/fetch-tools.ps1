# Downloads standalone yt-dlp.exe, ffmpeg.exe and ffprobe.exe (+ DLLs) into
# -Dest so they can be bundled with VidOps (it looks in .\tools next to VidOps.exe).
#
#   powershell -ExecutionPolicy Bypass -File scripts\fetch-tools.ps1 -Dest dist\VidOps\tools
param([Parameter(Mandatory = $true)][string]$Dest)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # Invoke-WebRequest is very slow with the progress bar

New-Item -ItemType Directory -Force -Path $Dest | Out-Null
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("vidops-tools-" + [guid]::NewGuid())
New-Item -ItemType Directory -Force -Path $work | Out-Null

try {
    Invoke-WebRequest -Uri 'https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe' `
        -OutFile (Join-Path $Dest 'yt-dlp.exe')

    # Shared build: ffmpeg.exe and ffprobe.exe share one set of DLLs (much smaller).
    $zip = Join-Path $work 'ffmpeg.zip'
    Invoke-WebRequest -Uri 'https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip' `
        -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $work -Force
    $bin = Get-ChildItem -Path $work -Directory -Filter 'ffmpeg-*' | Select-Object -First 1
    Copy-Item -Path (Join-Path $bin.FullName 'bin\*') -Include 'ffmpeg.exe', 'ffprobe.exe', '*.dll' -Destination $Dest

    & (Join-Path $Dest 'ffmpeg.exe') -hide_banner -version | Select-Object -First 1
    Write-Host "tools ready in $Dest"
}
finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
