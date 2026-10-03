#include "core/Options.h"

#include <QCoreApplication>

namespace vidops {

static QString tr(const char *s)
{
    return QCoreApplication::translate("vidops", s);
}

QString defaultFilenameTemplate()
{
    return QStringLiteral("%(title).150B [%(id)s].%(ext)s");
}

QList<OutputFormat> allFormats()
{
    return {OutputFormat::Mp4H264, OutputFormat::Mp4H265, OutputFormat::Mp4AV1,
            OutputFormat::Mp4Original, OutputFormat::AudioM4A, OutputFormat::AudioMP3};
}

QList<Encoder> allEncoders()
{
    return {Encoder::Software, Encoder::Nvenc, Encoder::QuickSync, Encoder::Amf,
            Encoder::VideoToolbox};
}

bool isAudioOnly(OutputFormat f)
{
    return f == OutputFormat::AudioM4A || f == OutputFormat::AudioMP3;
}

QString targetVideoCodec(OutputFormat f)
{
    switch (f) {
    case OutputFormat::Mp4H264: return QStringLiteral("h264");
    case OutputFormat::Mp4H265: return QStringLiteral("hevc");
    case OutputFormat::Mp4AV1: return QStringLiteral("av1");
    default: return {};
    }
}

bool convertsVideo(OutputFormat f)
{
    return !targetVideoCodec(f).isEmpty();
}

int defaultQuality(OutputFormat f)
{
    switch (f) {
    case OutputFormat::Mp4H264: return 23;
    case OutputFormat::Mp4H265: return 28;
    case OutputFormat::Mp4AV1: return 32;
    default: return 0;
    }
}

int effectiveQuality(const DownloadOptions &o)
{
    return o.quality >= 0 ? o.quality : defaultQuality(o.format);
}

QString formatLabel(OutputFormat f)
{
    switch (f) {
    case OutputFormat::Mp4H264: return QStringLiteral("MP4 · H.264");
    case OutputFormat::Mp4H265: return QStringLiteral("MP4 · H.265");
    case OutputFormat::Mp4AV1: return QStringLiteral("MP4 · AV1");
    case OutputFormat::Mp4Original: return tr("MP4 · Original");
    case OutputFormat::AudioM4A: return QStringLiteral("M4A · AAC");
    case OutputFormat::AudioMP3: return QStringLiteral("MP3");
    }
    return {};
}

QString formatDescription(OutputFormat f)
{
    switch (f) {
    case OutputFormat::Mp4H264: return tr("MP4 — H.264 / AVC (most compatible)");
    case OutputFormat::Mp4H265: return tr("MP4 — H.265 / HEVC (smaller files)");
    case OutputFormat::Mp4AV1: return tr("MP4 — AV1 (smallest, newer devices)");
    case OutputFormat::Mp4Original: return tr("MP4 — keep source codec (no re-encode)");
    case OutputFormat::AudioM4A: return tr("Audio only — M4A (AAC)");
    case OutputFormat::AudioMP3: return tr("Audio only — MP3");
    }
    return {};
}

QString encoderLabel(Encoder e)
{
    switch (e) {
    case Encoder::Software: return tr("CPU (software)");
    case Encoder::Nvenc: return tr("NVIDIA NVENC");
    case Encoder::QuickSync: return tr("Intel Quick Sync");
    case Encoder::Amf: return tr("AMD AMF");
    case Encoder::VideoToolbox: return tr("Apple VideoToolbox");
    }
    return {};
}

QString speedLabel(SpeedPreset s)
{
    switch (s) {
    case SpeedPreset::Fast: return tr("Fast");
    case SpeedPreset::Balanced: return tr("Balanced");
    case SpeedPreset::Quality: return tr("Best quality (slow)");
    }
    return {};
}

}  // namespace vidops
