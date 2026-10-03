#include "core/Ffmpeg.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace vidops::ffmpeg {

bool MediaInfo::isTenBit() const
{
    return pixFmt.contains(QLatin1String("10")) || pixFmt.contains(QLatin1String("12"));
}

QStringList ffprobeArgs(const QString &file)
{
    return {QStringLiteral("-v"), QStringLiteral("error"),
            QStringLiteral("-print_format"), QStringLiteral("json"),
            QStringLiteral("-show_streams"), QStringLiteral("-show_format"), file};
}

MediaInfo parseFfprobeJson(const QByteArray &json)
{
    MediaInfo info;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return info;

    const QJsonObject root = doc.object();
    double streamDuration = 0;
    for (const QJsonValue &v : root.value(QLatin1String("streams")).toArray()) {
        const QJsonObject s = v.toObject();
        const QString type = s.value(QLatin1String("codec_type")).toString();
        if (type == QLatin1String("video") && info.videoCodec.isEmpty()) {
            // Skip embedded cover art.
            if (s.value(QLatin1String("disposition")).toObject()
                    .value(QLatin1String("attached_pic")).toInt() == 1)
                continue;
            info.videoCodec = s.value(QLatin1String("codec_name")).toString();
            info.videoTag = s.value(QLatin1String("codec_tag_string")).toString();
            info.pixFmt = s.value(QLatin1String("pix_fmt")).toString();
            info.width = s.value(QLatin1String("width")).toInt();
            info.height = s.value(QLatin1String("height")).toInt();
            streamDuration = qMax(streamDuration,
                                  s.value(QLatin1String("duration")).toString().toDouble());
        } else if (type == QLatin1String("audio") && info.audioCodec.isEmpty()) {
            info.audioCodec = s.value(QLatin1String("codec_name")).toString();
            streamDuration = qMax(streamDuration,
                                  s.value(QLatin1String("duration")).toString().toDouble());
        }
    }
    info.durationSec = root.value(QLatin1String("format")).toObject()
                           .value(QLatin1String("duration")).toString().toDouble();
    if (info.durationSec <= 0)
        info.durationSec = streamDuration;
    info.valid = info.hasVideo() || info.hasAudio();
    return info;
}

QSet<QString> parseEncoderList(const QString &output)
{
    static const QRegularExpression re(QStringLiteral(R"(^\s*[VAS][A-Z.]{5}\s+(\S+))"));
    QSet<QString> names;
    for (const QString &line : output.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch() && m.captured(1) != QLatin1String("="))
            names.insert(m.captured(1));
    }
    return names;
}

static QStringList encoderCandidates(OutputFormat format, Encoder encoder)
{
    const QString codec = targetVideoCodec(format);
    if (codec.isEmpty())
        return {};
    const QString prefix = codec;  // h264 / hevc / av1
    switch (encoder) {
    case Encoder::Software:
        if (codec == QLatin1String("h264")) return {QStringLiteral("libx264")};
        if (codec == QLatin1String("hevc")) return {QStringLiteral("libx265")};
        return {QStringLiteral("libsvtav1"), QStringLiteral("libaom-av1")};
    case Encoder::Nvenc: return {prefix + QStringLiteral("_nvenc")};
    case Encoder::QuickSync: return {prefix + QStringLiteral("_qsv")};
    case Encoder::Amf: return {prefix + QStringLiteral("_amf")};
    case Encoder::VideoToolbox:
        if (codec == QLatin1String("av1")) return {};
        return {prefix + QStringLiteral("_videotoolbox")};
    }
    return {};
}

bool isHardwareEncoder(const QString &name)
{
    return name.endsWith(QLatin1String("_nvenc")) || name.endsWith(QLatin1String("_qsv"))
        || name.endsWith(QLatin1String("_amf")) || name.endsWith(QLatin1String("_videotoolbox"));
}

QString pickVideoEncoder(OutputFormat format, Encoder encoder, const QSet<QString> &available,
                         QString *note)
{
    auto firstAvailable = [&available](const QStringList &names) -> QString {
        for (const QString &n : names) {
            if (available.isEmpty() || available.contains(n))
                return n;
        }
        return {};
    };

    QString name = firstAvailable(encoderCandidates(format, encoder));
    if (name.isEmpty() && encoder != Encoder::Software) {
        name = firstAvailable(encoderCandidates(format, Encoder::Software));
        if (note && !name.isEmpty())
            *note = QStringLiteral("%1 has no encoder for this codec in this ffmpeg build, using %2")
                        .arg(encoderLabel(encoder), name);
    }
    return name;
}

TranscodePlan planTranscode(const MediaInfo &info, const DownloadOptions &o,
                            const QSet<QString> &availableEncoders)
{
    TranscodePlan plan;
    const QString target = targetVideoCodec(o.format);
    if (target.isEmpty() || !info.valid)
        return plan;

    if (info.hasVideo()) {
        if (o.forceReencode) {
            plan.video = true;
            plan.reasons << QStringLiteral("re-encode forced");
        } else if (info.videoCodec != target) {
            plan.video = true;
            plan.reasons << QStringLiteral("video is %1, need %2").arg(info.videoCodec, target);
        } else if (target == QLatin1String("h264") && info.isTenBit()) {
            // 10-bit H.264 (High 10) does not play on most hardware decoders.
            plan.video = true;
            plan.reasons << QStringLiteral("10-bit H.264 is not widely supported");
        }
    }
    if (info.hasAudio() && info.audioCodec != QLatin1String("aac")
        && info.audioCodec != QLatin1String("mp3")) {
        plan.audio = true;
        plan.reasons << QStringLiteral("audio is %1, need aac").arg(info.audioCodec);
    }
    // HEVC tagged hev1 is rejected by Apple players; a remux to hvc1 fixes it.
    const bool retag = !plan.video && target == QLatin1String("hevc")
                       && info.videoTag == QLatin1String("hev1");
    if (retag)
        plan.reasons << QStringLiteral("retag HEVC as hvc1");

    if (plan.video) {
        QString note;
        plan.videoEncoder = pickVideoEncoder(o.format, o.encoder, availableEncoders, &note);
        if (!note.isEmpty())
            plan.reasons << note;
        if (plan.videoEncoder.isEmpty()) {
            plan.error = QStringLiteral("this ffmpeg build has no %1 encoder").arg(target);
            return plan;
        }
    }
    plan.needed = plan.video || plan.audio || retag;
    return plan;
}

QStringList encoderArgs(const QString &enc, int q, SpeedPreset speed)
{
    const int s = static_cast<int>(speed);  // 0 fast, 1 balanced, 2 quality
    auto pick = [s](const char *fast, const char *balanced, const char *quality) {
        return QString::fromLatin1(s == 0 ? fast : s == 1 ? balanced : quality);
    };
    const QString qs = QString::number(q);

    if (enc == QLatin1String("libx264") || enc == QLatin1String("libx265")) {
        QStringList a{QStringLiteral("-crf"), qs, QStringLiteral("-preset"),
                      pick("veryfast", "medium", "slow")};
        if (enc == QLatin1String("libx265"))
            a << QStringLiteral("-x265-params") << QStringLiteral("log-level=error");
        return a;
    }
    if (enc == QLatin1String("libsvtav1"))
        return {QStringLiteral("-crf"), qs, QStringLiteral("-preset"), pick("10", "8", "5")};
    if (enc == QLatin1String("libaom-av1"))
        return {QStringLiteral("-crf"), qs, QStringLiteral("-b:v"), QStringLiteral("0"),
                QStringLiteral("-cpu-used"), pick("8", "6", "4"),
                QStringLiteral("-row-mt"), QStringLiteral("1")};
    if (enc.endsWith(QLatin1String("_nvenc")))
        return {QStringLiteral("-preset"), pick("p2", "p4", "p6"), QStringLiteral("-rc"),
                QStringLiteral("vbr"), QStringLiteral("-cq"), qs, QStringLiteral("-b:v"),
                QStringLiteral("0")};
    if (enc.endsWith(QLatin1String("_qsv")))
        return {QStringLiteral("-preset"), pick("veryfast", "medium", "slow"),
                QStringLiteral("-global_quality"), qs};
    if (enc.endsWith(QLatin1String("_amf"))) {
        QStringList a{QStringLiteral("-quality"), pick("speed", "balanced", "quality"),
                      QStringLiteral("-rc"), QStringLiteral("cqp"),
                      QStringLiteral("-qp_i"), qs, QStringLiteral("-qp_p"), qs};
        if (enc.startsWith(QLatin1String("h264")))
            a << QStringLiteral("-qp_b") << qs;
        return a;
    }
    if (enc.endsWith(QLatin1String("_videotoolbox"))) {
        // VideoToolbox uses 1..100, higher is better.
        const int vtq = qBound(1, 100 - int(q * 1.5 + 0.5), 100);
        return {QStringLiteral("-q:v"), QString::number(vtq)};
    }
    return {};
}

static QString pixelFormatFor(const QString &enc, const QString &targetCodec, bool sourceTenBit)
{
    const bool software = !isHardwareEncoder(enc);
    // Keep 10-bit (HDR) only for HEVC/AV1 with software encoders; everything
    // else gets the universally supported 8-bit 4:2:0.
    if (software && sourceTenBit && targetCodec != QLatin1String("h264"))
        return QStringLiteral("yuv420p10le");
    if (enc.endsWith(QLatin1String("_qsv")) || enc.endsWith(QLatin1String("_amf"))
        || enc.endsWith(QLatin1String("_videotoolbox")))
        return QStringLiteral("nv12");
    return QStringLiteral("yuv420p");
}

QStringList transcodeArgs(const TranscodePlan &plan, const MediaInfo &info,
                          const DownloadOptions &o, const QString &input, const QString &output)
{
    const QString target = targetVideoCodec(o.format);
    QStringList a{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                  QStringLiteral("-i"), input,
                  QStringLiteral("-map"), QStringLiteral("0:v:0?"),
                  QStringLiteral("-map"), QStringLiteral("0:a:0?"),
                  QStringLiteral("-map_metadata"), QStringLiteral("0")};

    if (plan.video) {
        a << QStringLiteral("-c:v") << plan.videoEncoder
          << encoderArgs(plan.videoEncoder, effectiveQuality(o), o.speed)
          << QStringLiteral("-pix_fmt")
          << pixelFormatFor(plan.videoEncoder, target, info.isTenBit());
    } else {
        a << QStringLiteral("-c:v") << QStringLiteral("copy");
    }
    if (target == QLatin1String("hevc"))
        a << QStringLiteral("-tag:v") << QStringLiteral("hvc1");

    if (plan.audio)
        a << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a")
          << QStringLiteral("%1k").arg(o.audioBitrateKbps);
    else
        a << QStringLiteral("-c:a") << QStringLiteral("copy");

    a << QStringLiteral("-movflags") << QStringLiteral("+faststart")
      << QStringLiteral("-progress") << QStringLiteral("pipe:1") << QStringLiteral("-nostats")
      << output;
    return a;
}

double parseProgressSeconds(const QString &rawLine)
{
    const QString line = rawLine.trimmed();
    // out_time_ms is (despite the name) also in microseconds.
    for (const char *key : {"out_time_us=", "out_time_ms="}) {
        if (line.startsWith(QLatin1String(key))) {
            bool ok = false;
            const qint64 us = line.mid(int(qstrlen(key))).toLongLong(&ok);
            return ok && us >= 0 ? us / 1e6 : -1;
        }
    }
    return -1;
}

}  // namespace vidops::ffmpeg
