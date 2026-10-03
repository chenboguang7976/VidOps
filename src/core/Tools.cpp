#include "core/Tools.h"

#include "core/Ffmpeg.h"
#include "core/Process.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

namespace vidops {

QString locateTool(const QString &baseName, const QString &overridePath)
{
    if (!overridePath.isEmpty()) {
        const QFileInfo fi(overridePath);
        return fi.isFile() && fi.isExecutable() ? fi.absoluteFilePath() : QString();
    }

#ifdef Q_OS_WIN
    const QString exe = baseName + QStringLiteral(".exe");
#else
    const QString exe = baseName;
#endif
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList dirs{appDir, appDir + QStringLiteral("/tools")};
#ifdef Q_OS_MACOS
    dirs << appDir + QStringLiteral("/../Resources/tools");
#endif
    for (const QString &dir : dirs) {
        const QFileInfo fi(QDir(dir).filePath(exe));
        if (fi.isFile() && fi.isExecutable())
            return fi.canonicalFilePath();
    }

    QString found = QStandardPaths::findExecutable(baseName);
#ifndef Q_OS_WIN
    // GUI apps on macOS do not inherit the shell PATH.
    if (found.isEmpty())
        found = QStandardPaths::findExecutable(
            baseName, {QStringLiteral("/opt/homebrew/bin"), QStringLiteral("/usr/local/bin"),
                       QDir::homePath() + QStringLiteral("/.local/bin")});
#endif
    return found;
}

void runCapture(QObject *context, const QString &program, const QStringList &args,
                int timeoutMs, std::function<void(bool, const QString &)> done)
{
    if (program.isEmpty()) {
        done(false, {});
        return;
    }
    QProcess *p = createToolProcess(context);
    p->setProcessChannelMode(QProcess::MergedChannels);
    auto called = std::make_shared<bool>(false);
    auto finish = [p, called, done](bool ok) {
        if (*called)
            return;
        *called = true;
        const QString out = QString::fromUtf8(p->readAll());
        p->deleteLater();
        done(ok, out);
    };
    QObject::connect(p, &QProcess::finished, p, [p, finish](int code, QProcess::ExitStatus st) {
        Q_UNUSED(p);
        finish(st == QProcess::NormalExit && code == 0);
    });
    QObject::connect(p, &QProcess::errorOccurred, p, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(false);
    });
    QTimer::singleShot(timeoutMs, p, [p] { terminateProcessTree(p); });
    p->start(program, args);
}

void ToolDetector::detect(const QString &ytdlpOverride, const QString &ffmpegOverride,
                          const QString &ffprobeOverride)
{
    auto tools = std::make_shared<ToolSet>();
    tools->ytdlp = locateTool(QStringLiteral("yt-dlp"), ytdlpOverride);
    tools->ffmpeg = locateTool(QStringLiteral("ffmpeg"), ffmpegOverride);
    QString probeOverride = ffprobeOverride;
    if (probeOverride.isEmpty() && !ffmpegOverride.isEmpty()) {
        // Look for ffprobe next to a user-chosen ffmpeg.
        const QFileInfo ff(ffmpegOverride);
        const QString candidate = ff.dir().filePath(
            QStringLiteral("ffprobe") + (ff.suffix().isEmpty() ? QString() : QLatin1Char('.') + ff.suffix()));
        if (QFileInfo(candidate).isExecutable())
            probeOverride = candidate;
    }
    tools->ffprobe = locateTool(QStringLiteral("ffprobe"), probeOverride);

    runCapture(this, tools->ytdlp, {QStringLiteral("--version")}, 20000,
               [this, tools](bool ok, const QString &out) {
        if (ok)
            tools->ytdlpVersion = out.trimmed().section(QLatin1Char('\n'), -1).trimmed();
        runCapture(this, tools->ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-version")}, 10000,
                   [this, tools](bool ok, const QString &out) {
            if (ok) {
                // "ffmpeg version 6.1.1-3ubuntu5 Copyright ..." -> "6.1.1-3ubuntu5"
                tools->ffmpegVersion = out.section(QLatin1Char('\n'), 0, 0).section(QLatin1Char(' '), 2, 2);
            }
            runCapture(this, tools->ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")}, 10000,
                       [this, tools](bool ok, const QString &out) {
                if (ok)
                    tools->encoders = ffmpeg::parseEncoderList(out);
                emit detected(*tools);
            });
        });
    });
}

}  // namespace vidops
