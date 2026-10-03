#include "core/AppSettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace vidops {

QString AppSettings::defaultOutputDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    return QDir(base).filePath(QStringLiteral("VidOps"));
}

template <typename E>
static E enumValue(const QSettings &s, const char *key, E fallback, E last)
{
    const int v = s.value(QLatin1String(key), static_cast<int>(fallback)).toInt();
    return v >= 0 && v <= static_cast<int>(last) ? static_cast<E>(v) : fallback;
}

AppSettings AppSettings::load()
{
    QSettings s;
    AppSettings a;
    DownloadOptions &o = a.options;
    o.format = enumValue(s, "download/format", o.format, OutputFormat::AudioMP3);
    o.maxHeight = s.value(QStringLiteral("download/maxHeight"), o.maxHeight).toInt();
    o.forceReencode = s.value(QStringLiteral("download/forceReencode"), false).toBool();
    o.playlist = s.value(QStringLiteral("download/playlist"), false).toBool();
    o.encoder = enumValue(s, "encode/encoder", o.encoder, Encoder::VideoToolbox);
    o.speed = enumValue(s, "encode/speed", o.speed, SpeedPreset::Quality);
    o.quality = s.value(QStringLiteral("encode/quality"), -1).toInt();
    o.audioBitrateKbps = s.value(QStringLiteral("encode/audioBitrate"), 192).toInt();
    o.keepOriginal = s.value(QStringLiteral("encode/keepOriginal"), false).toBool();
    o.outputDir = s.value(QStringLiteral("download/outputDir"), defaultOutputDir()).toString();
    o.filenameTemplate = s.value(QStringLiteral("download/filenameTemplate"), defaultFilenameTemplate()).toString();
    o.cookiesBrowser = s.value(QStringLiteral("download/cookiesBrowser")).toString();
    o.cookiesFile = s.value(QStringLiteral("download/cookiesFile")).toString();
    a.ytdlpPath = s.value(QStringLiteral("tools/ytdlp")).toString();
    a.ffmpegPath = s.value(QStringLiteral("tools/ffmpeg")).toString();
    a.ffprobePath = s.value(QStringLiteral("tools/ffprobe")).toString();
    a.maxConcurrent = qBound(1, s.value(QStringLiteral("download/maxConcurrent"), 2).toInt(), 8);
    return a;
}

void AppSettings::save() const
{
    QSettings s;
    const DownloadOptions &o = options;
    s.setValue(QStringLiteral("download/format"), static_cast<int>(o.format));
    s.setValue(QStringLiteral("download/maxHeight"), o.maxHeight);
    s.setValue(QStringLiteral("download/forceReencode"), o.forceReencode);
    s.setValue(QStringLiteral("download/playlist"), o.playlist);
    s.setValue(QStringLiteral("encode/encoder"), static_cast<int>(o.encoder));
    s.setValue(QStringLiteral("encode/speed"), static_cast<int>(o.speed));
    s.setValue(QStringLiteral("encode/quality"), o.quality);
    s.setValue(QStringLiteral("encode/audioBitrate"), o.audioBitrateKbps);
    s.setValue(QStringLiteral("encode/keepOriginal"), o.keepOriginal);
    s.setValue(QStringLiteral("download/outputDir"), o.outputDir);
    s.setValue(QStringLiteral("download/filenameTemplate"), o.filenameTemplate);
    s.setValue(QStringLiteral("download/cookiesBrowser"), o.cookiesBrowser);
    s.setValue(QStringLiteral("download/cookiesFile"), o.cookiesFile);
    s.setValue(QStringLiteral("tools/ytdlp"), ytdlpPath);
    s.setValue(QStringLiteral("tools/ffmpeg"), ffmpegPath);
    s.setValue(QStringLiteral("tools/ffprobe"), ffprobePath);
    s.setValue(QStringLiteral("download/maxConcurrent"), maxConcurrent);
}

}  // namespace vidops
