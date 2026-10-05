#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace vidops {

// What the user wants to end up with on disk.
enum class OutputFormat {
    Mp4H264,      // MP4, H.264/AVC + AAC — plays everywhere
    Mp4H265,      // MP4, H.265/HEVC + AAC — smaller files
    Mp4AV1,       // MP4, AV1 + AAC — smallest, newer devices
    Mp4Original,  // MP4 container, keep whatever codec the site serves
    AudioM4A,     // audio only, AAC in .m4a
    AudioMP3,     // audio only, .mp3
};

// Which ffmpeg encoder family to use when a re-encode is needed.
enum class Encoder {
    Software,      // libx264 / libx265 / libsvtav1
    Nvenc,         // NVIDIA
    QuickSync,     // Intel
    Amf,           // AMD
    VideoToolbox,  // Apple
};

enum class SpeedPreset { Fast, Balanced, Quality };

struct DownloadOptions {
    OutputFormat format = OutputFormat::Mp4H264;
    int maxHeight = 1080;  // 0 = best available
    bool forceReencode = false;
    bool playlist = false;
    Encoder encoder = Encoder::Software;
    SpeedPreset speed = SpeedPreset::Balanced;
    int quality = -1;  // CRF-like value (lower = better), -1 = per-codec default
    int audioBitrateKbps = 192;
    bool keepOriginal = false;
    QString outputDir;
    QString filenameTemplate;  // yt-dlp output template, empty = default
    QString cookiesBrowser;    // e.g. "chrome", empty = none
    QString cookiesFile;       // Netscape cookies.txt, takes precedence over browser
    QString ffmpegPath;        // forwarded to yt-dlp --ffmpeg-location
    QString denoPath;          // JavaScript runtime for YouTube challenges, empty = let yt-dlp look
    QStringList extraArgs;     // appended to the yt-dlp command line (Settings → extra arguments)
};

QString defaultFilenameTemplate();

QList<OutputFormat> allFormats();
QList<Encoder> allEncoders();

bool isAudioOnly(OutputFormat f);
// ffprobe codec name the file must have, empty when no conversion is wanted.
QString targetVideoCodec(OutputFormat f);
bool convertsVideo(OutputFormat f);
int defaultQuality(OutputFormat f);
int effectiveQuality(const DownloadOptions &o);

QString formatLabel(OutputFormat f);        // short, for the queue table
QString formatDescription(OutputFormat f);  // long, for the combo box
QString encoderLabel(Encoder e);
QString speedLabel(SpeedPreset s);

}  // namespace vidops
