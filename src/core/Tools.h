#pragma once

#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

namespace vidops {

struct ToolSet {
    QString ytdlp;
    QString ffmpeg;
    QString ffprobe;
    QString ytdlpVersion;
    QString ffmpegVersion;
    QSet<QString> encoders;  // from `ffmpeg -encoders`, empty if unknown
};

// Looks next to the executable (and in ./tools), then on PATH and the usual
// Homebrew / ~/.local locations. `overridePath` wins when set.
QString locateTool(const QString &baseName, const QString &overridePath = {});

// Runs a short command and hands its stdout+stderr to `done`.
void runCapture(QObject *context, const QString &program, const QStringList &args,
                int timeoutMs, std::function<void(bool ok, const QString &output)> done);

// Finds the tools and asks them for versions/capabilities, asynchronously.
class ToolDetector : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    void detect(const QString &ytdlpOverride, const QString &ffmpegOverride,
                const QString &ffprobeOverride);

signals:
    void detected(const vidops::ToolSet &tools);
};

}  // namespace vidops
