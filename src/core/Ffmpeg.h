#pragma once

#include "core/Options.h"

#include <QByteArray>
#include <QSet>
#include <QStringList>

// ffprobe parsing and planning of the ffmpeg conversion that turns whatever
// the site served into a standard MP4 (H.264/H.265/AV1 + AAC).
namespace vidops::ffmpeg {

struct MediaInfo {
    bool valid = false;
    QString videoCodec;  // ffprobe codec_name: h264, hevc, av1, vp9, ...
    QString videoTag;    // codec_tag_string: avc1, hvc1, hev1, ...
    QString pixFmt;
    QString audioCodec;  // aac, opus, mp3, ...
    int width = 0;
    int height = 0;
    double durationSec = 0;

    bool hasVideo() const { return !videoCodec.isEmpty(); }
    bool hasAudio() const { return !audioCodec.isEmpty(); }
    bool isTenBit() const;
};

QStringList ffprobeArgs(const QString &file);
MediaInfo parseFfprobeJson(const QByteArray &json);

// Parses `ffmpeg -hide_banner -encoders` output into a set of encoder names.
QSet<QString> parseEncoderList(const QString &output);

// ffmpeg encoder name for the format/encoder pair, or empty if none fits.
// When `available` is non-empty, unavailable hardware encoders fall back to
// software and `note` explains why.
QString pickVideoEncoder(OutputFormat format, Encoder encoder, const QSet<QString> &available,
                         QString *note = nullptr);
bool isHardwareEncoder(const QString &encoderName);

struct TranscodePlan {
    bool needed = false;
    bool video = false;  // re-encode video (otherwise stream copy)
    bool audio = false;  // re-encode audio to AAC (otherwise stream copy)
    QString videoEncoder;
    QString error;       // non-empty: cannot produce the requested format
    QStringList reasons;  // human readable, for the log
};

TranscodePlan planTranscode(const MediaInfo &info, const DownloadOptions &o,
                            const QSet<QString> &availableEncoders);

QStringList encoderArgs(const QString &encoderName, int quality, SpeedPreset speed);
QStringList transcodeArgs(const TranscodePlan &plan, const MediaInfo &info,
                          const DownloadOptions &o, const QString &input,
                          const QString &output);

// `-progress pipe:1` line -> seconds of output written, or -1.
double parseProgressSeconds(const QString &line);

}  // namespace vidops::ffmpeg
