#include "core/DownloadJob.h"

#include "core/Process.h"
#include "core/YtDlp.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace vidops {

DownloadJob::DownloadJob(int id, const QString &url, const DownloadOptions &options, QObject *parent)
    : QObject(parent), m_id(id), m_url(url), m_options(options), m_title(url)
{
}

DownloadJob::~DownloadJob()
{
    if (m_proc) {
        m_proc->disconnect(this);
        terminateProcessTree(m_proc);
        m_proc->waitForFinished(2000);
    }
    if (!m_tempFile.isEmpty())
        QFile::remove(m_tempFile);
    if (!m_pathsFile.isEmpty())
        QFile::remove(m_pathsFile);
}

bool DownloadJob::isActive() const
{
    return m_state == State::Downloading || m_state == State::PostProcessing
        || m_state == State::Converting;
}

bool DownloadJob::isFinished() const
{
    return m_state == State::Done || m_state == State::Failed || m_state == State::Cancelled;
}

QString DownloadJob::statusText() const
{
    switch (m_state) {
    case State::Queued: return tr("Queued");
    case State::Downloading: return m_detail.isEmpty() ? tr("Downloading") : m_detail;
    case State::PostProcessing: return m_detail.isEmpty() ? tr("Processing") : m_detail;
    case State::Converting: return m_detail.isEmpty() ? tr("Converting") : m_detail;
    case State::Done: return m_warning.isEmpty() ? tr("Done") : tr("Done (with warnings)");
    case State::Failed: return tr("Failed: %1").arg(m_error);
    case State::Cancelled: return tr("Cancelled");
    }
    return {};
}

void DownloadJob::setState(State s, const QString &detail)
{
    m_state = s;
    m_detail = detail;
    emit changed();
}

void DownloadJob::log(const QString &line)
{
    emit logMessage(QStringLiteral("[#%1] %2").arg(m_id).arg(line));
}

void DownloadJob::fail(const QString &message)
{
    if (!m_tempFile.isEmpty()) {
        QFile::remove(m_tempFile);
        m_tempFile.clear();
    }
    m_error = message;
    m_speed.clear();
    m_eta.clear();
    log(tr("Failed: %1").arg(message));
    setState(State::Failed);
    emit finished();
}

void DownloadJob::finishCancelled()
{
    if (!m_tempFile.isEmpty()) {
        QFile::remove(m_tempFile);
        m_tempFile.clear();
    }
    m_speed.clear();
    m_eta.clear();
    log(tr("Cancelled"));
    setState(State::Cancelled);
    emit finished();
}

void DownloadJob::resetForRetry()
{
    if (isActive())
        return;
    m_error.clear();
    m_warning.clear();
    m_lastYtDlpError.clear();
    m_progress = 0;
    m_speed.clear();
    m_eta.clear();
    m_downloaded.clear();
    m_outputs.clear();
    m_fileIndex = 0;
    m_retries = 0;
    m_hwFallbackUsed = false;
    m_cancelRequested = false;
    setState(State::Queued);
}

void DownloadJob::cancel()
{
    if (isFinished())
        return;
    m_cancelRequested = true;
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        terminateProcessTree(m_proc);  // the finished handler reports Cancelled
    else
        finishCancelled();
}

QProcess *DownloadJob::newProcess()
{
    if (m_proc)
        m_proc->deleteLater();
    m_stdoutBuf.clear();
    m_stderrBuf.clear();
    m_stderrTail.clear();
    m_proc = createToolProcess(this);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            const QString program = m_proc->program();
            m_proc->deleteLater();
            m_proc = nullptr;
            if (m_cancelRequested)
                finishCancelled();
            else
                fail(tr("could not start %1").arg(QFileInfo(program).fileName()));
        }
    });
    return m_proc;
}

void DownloadJob::drainLines(QProcess *proc, QByteArray &buffer, bool flush,
                             const std::function<void(const QString &)> &handler)
{
    Q_UNUSED(proc);
    for (;;) {
        int idx = -1;
        for (int i = 0; i < buffer.size(); ++i) {
            if (buffer.at(i) == '\n' || buffer.at(i) == '\r') {
                idx = i;
                break;
            }
        }
        if (idx < 0)
            break;
        const QByteArray line = buffer.left(idx);
        buffer.remove(0, idx + 1);
        if (!line.trimmed().isEmpty())
            handler(QString::fromUtf8(line));
    }
    if (flush && !buffer.trimmed().isEmpty()) {
        handler(QString::fromUtf8(buffer));
        buffer.clear();
    }
}

// ---------------------------------------------------------------- yt-dlp

void DownloadJob::start(const ToolSet &tools)
{
    m_tools = tools;
    if (m_tools.ytdlp.isEmpty()) {
        fail(tr("yt-dlp was not found. Install it or set its path in Settings."));
        return;
    }
    if (convertsVideo(m_options.format) || isAudioOnly(m_options.format)
        || m_options.format == OutputFormat::Mp4Original) {
        if (m_tools.ffmpeg.isEmpty()) {
            fail(tr("ffmpeg was not found. Install it or set its path in Settings."));
            return;
        }
    }
    if (!m_options.outputDir.isEmpty() && !QDir().mkpath(m_options.outputDir)) {
        fail(tr("cannot create folder %1").arg(m_options.outputDir));
        return;
    }
    m_retries = 0;
    runYtDlp({});
}

void DownloadJob::runYtDlp(const QStringList &retryArgs)
{
    DownloadOptions o = m_options;
    o.extraArgs << retryArgs;
    if (o.ffmpegPath.isEmpty())
        o.ffmpegPath = m_tools.ffmpeg;
    if (o.denoPath.isEmpty())
        o.denoPath = m_tools.deno;
    if (!m_pathsFile.isEmpty())
        QFile::remove(m_pathsFile);
    m_pathsFile = QDir::temp().filePath(
        QStringLiteral("vidops-%1-%2.txt")
            .arg(QCoreApplication::applicationPid())
            .arg(QUuid::createUuid().toString(QUuid::Id128)));
    const QStringList args = ytdlp::buildArgs(o, m_url, m_pathsFile);

    QProcess *p = newProcess();
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p] {
        m_stdoutBuf += p->readAllStandardOutput();
        drainLines(p, m_stdoutBuf, false, [this](const QString &l) { handleYtDlpLine(l); });
    });
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus) {
        m_stdoutBuf += p->readAllStandardOutput();
        drainLines(p, m_stdoutBuf, true, [this](const QString &l) { handleYtDlpLine(l); });
        onYtDlpFinished(code);
    });

    m_progress = 0;
    log(QStringLiteral("yt-dlp %1").arg(args.join(QLatin1Char(' '))));
    setState(State::Downloading, tr("Starting"));
    p->start(m_tools.ytdlp, args);
}

void DownloadJob::handleYtDlpLine(const QString &line)
{
    const ytdlp::Event ev = ytdlp::parseLine(line);
    switch (ev.type) {
    case ytdlp::Event::Progress:
        if (ev.percent >= 0)
            m_progress = ev.percent;
        m_speed = ev.speed;
        m_eta = ev.eta;
        m_state = State::Downloading;
        m_detail = tr("Downloading");
        emit changed();
        break;
    case ytdlp::Event::PostProcess: {
        QString what = ev.text;
        if (what == QLatin1String("Merger"))
            what = tr("Merging");
        else if (what.startsWith(QLatin1String("FFmpegExtractAudio")))
            what = tr("Extracting audio");
        else if (what.startsWith(QLatin1String("FFmpegVideoRemuxer")))
            what = tr("Remuxing");
        else if (what.startsWith(QLatin1String("FFmpegMetadata")))
            what = tr("Writing metadata");
        m_speed.clear();
        m_eta.clear();
        setState(State::PostProcessing, what);
        break;
    }
    case ytdlp::Event::Title:
        if (m_title != ev.text) {
            m_title = ev.text;
            log(tr("Title: %1").arg(ev.text));
            emit changed();
        }
        break;
    case ytdlp::Event::File:
        if (!m_downloaded.contains(ev.text))
            m_downloaded << ev.text;
        log(tr("Downloaded: %1").arg(ev.text));
        break;
    case ytdlp::Event::Error:
        m_lastYtDlpError = ev.text;
        log(QStringLiteral("ERROR: ") + ev.text);
        break;
    case ytdlp::Event::Warning:
        log(QStringLiteral("WARNING: ") + ev.text);
        break;
    case ytdlp::Event::None:
        log(ev.text);
        break;
    }
}

void DownloadJob::onYtDlpFinished(int exitCode)
{
    m_proc->deleteLater();
    m_proc = nullptr;

    // Paths from the UTF-8 file are exact; the console copies may have lost
    // characters to the Windows code page.
    const QStringList exact = ytdlp::readPathsFile(m_pathsFile);
    QFile::remove(m_pathsFile);
    m_pathsFile.clear();
    if (!exact.isEmpty())
        m_downloaded = exact;

    if (m_cancelRequested) {
        finishCancelled();
        return;
    }
    if (m_downloaded.isEmpty()) {
        const QStringList retry = ytdlp::retryArgs(m_url, m_lastYtDlpError, m_retries);
        if (!retry.isEmpty()) {
            ++m_retries;
            log(tr("YouTube refused the stream (%1), retrying with %2")
                    .arg(m_lastYtDlpError, retry.join(QLatin1Char(' '))));
            m_lastYtDlpError.clear();
            runYtDlp(retry);
            return;
        }
        fail(m_lastYtDlpError.isEmpty() ? tr("yt-dlp exited with code %1").arg(exitCode)
                                        : m_lastYtDlpError);
        return;
    }
    if (exitCode != 0) {
        m_warning = m_lastYtDlpError.isEmpty() ? tr("some items failed") : m_lastYtDlpError;
        log(tr("Warning: %1").arg(m_warning));
    }
    m_speed.clear();
    m_eta.clear();
    m_fileIndex = 0;
    processNextFile();
}

// ---------------------------------------------------------------- conversion

void DownloadJob::processNextFile()
{
    if (m_cancelRequested) {
        finishCancelled();
        return;
    }
    if (m_fileIndex >= m_downloaded.size()) {
        m_progress = 100;
        log(tr("Finished"));
        setState(State::Done);
        emit finished();
        return;
    }
    const QString file = m_downloaded.at(m_fileIndex);
    if (!QFileInfo::exists(file)) {
        fail(tr("downloaded file not found: %1").arg(file));
        return;
    }
    if (!convertsVideo(m_options.format)) {
        m_outputs << file;
        ++m_fileIndex;
        processNextFile();
        return;
    }
    if (m_tools.ffprobe.isEmpty()) {
        fail(tr("ffprobe was not found, cannot check the video codec"));
        return;
    }
    startProbe(file);
}

void DownloadJob::startProbe(const QString &file)
{
    QProcess *p = newProcess();
    connect(p, &QProcess::finished, this, [this, p, file](int code, QProcess::ExitStatus st) {
        const QByteArray json = p->readAllStandardOutput();
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        p->deleteLater();
        m_proc = nullptr;
        if (m_cancelRequested) {
            finishCancelled();
            return;
        }
        const ffmpeg::MediaInfo info = ffmpeg::parseFfprobeJson(json);
        if (st != QProcess::NormalExit || code != 0 || !info.valid) {
            fail(tr("cannot read %1: %2").arg(QFileInfo(file).fileName(), err));
            return;
        }
        log(tr("Source: video=%1 %2x%3 %4, audio=%5")
                .arg(info.videoCodec.isEmpty() ? QStringLiteral("-") : info.videoCodec)
                .arg(info.width).arg(info.height).arg(info.pixFmt)
                .arg(info.audioCodec.isEmpty() ? QStringLiteral("-") : info.audioCodec));

        const ffmpeg::TranscodePlan plan = ffmpeg::planTranscode(info, m_options, m_tools.encoders);
        if (!plan.error.isEmpty()) {
            fail(plan.error);
            return;
        }
        if (!plan.needed) {
            log(tr("Already %1, no conversion needed").arg(formatLabel(m_options.format)));
            m_outputs << file;
            ++m_fileIndex;
            processNextFile();
            return;
        }
        startTranscode(file, info, plan);
    });
    setState(State::Converting, tr("Analyzing"));
    m_progress = -1;
    emit changed();
    p->start(m_tools.ffprobe, ffmpeg::ffprobeArgs(file));
}

QString DownloadJob::convertedPath(const QString &input) const
{
    const QFileInfo fi(input);
    const QString base = fi.dir().filePath(fi.completeBaseName());
    if (m_options.keepOriginal) {
        const QString codec = targetVideoCodec(m_options.format);
        return base + QLatin1Char('.') + codec + QStringLiteral(".mp4");
    }
    return base + QStringLiteral(".mp4");
}

void DownloadJob::startTranscode(const QString &file, const ffmpeg::MediaInfo &info,
                                 const ffmpeg::TranscodePlan &plan)
{
    const QString finalPath = convertedPath(file);
    const QFileInfo fi(file);
    m_tempFile = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".vidops-tmp.mp4"));
    const QStringList args = ffmpeg::transcodeArgs(plan, info, m_options, file, m_tempFile);

    log(tr("Converting (%1)").arg(plan.reasons.join(QStringLiteral("; "))));
    log(QStringLiteral("ffmpeg %1").arg(args.join(QLatin1Char(' '))));

    const int total = m_downloaded.size();
    const QString what = plan.video ? plan.videoEncoder : tr("remux");
    const QString detail = total > 1 ? tr("Converting %1/%2 (%3)").arg(m_fileIndex + 1).arg(total).arg(what)
                                     : tr("Converting (%1)").arg(what);
    const double duration = info.durationSec;

    QProcess *p = newProcess();
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p, duration] {
        m_stdoutBuf += p->readAllStandardOutput();
        drainLines(p, m_stdoutBuf, false, [this, duration](const QString &l) {
            const double sec = ffmpeg::parseProgressSeconds(l);
            if (sec >= 0 && duration > 0) {
                m_progress = qBound(0.0, sec / duration * 100.0, 100.0);
                emit changed();
            } else if (l.startsWith(QLatin1String("speed="))) {
                const QString sp = l.mid(6).trimmed();
                m_speed = sp == QLatin1String("N/A") ? QString() : sp;
            }
        });
    });
    connect(p, &QProcess::readyReadStandardError, this, [this, p] {
        m_stderrBuf += p->readAllStandardError();
        drainLines(p, m_stderrBuf, false, [this](const QString &l) {
            m_stderrTail << l;
            if (m_stderrTail.size() > 30)
                m_stderrTail.removeFirst();
        });
    });
    connect(p, &QProcess::finished, this, [this, p, file, finalPath, info, plan](int code, QProcess::ExitStatus st) {
        m_stderrBuf += p->readAllStandardError();
        drainLines(p, m_stderrBuf, true, [this](const QString &l) { m_stderrTail << l; });
        p->deleteLater();
        m_proc = nullptr;
        m_speed.clear();
        if (m_cancelRequested) {
            finishCancelled();
            return;
        }
        if (st != QProcess::NormalExit || code != 0) {
            for (const QString &l : std::as_const(m_stderrTail))
                log(QStringLiteral("ffmpeg: ") + l);
            QFile::remove(m_tempFile);
            m_tempFile.clear();
            // A listed hardware encoder may still have no usable GPU/driver.
            if (plan.video && ffmpeg::isHardwareEncoder(plan.videoEncoder) && !m_hwFallbackUsed) {
                m_hwFallbackUsed = true;
                DownloadOptions sw = m_options;
                sw.encoder = Encoder::Software;
                const ffmpeg::TranscodePlan swPlan = ffmpeg::planTranscode(info, sw, m_tools.encoders);
                if (swPlan.error.isEmpty() && swPlan.needed) {
                    log(tr("%1 failed, retrying with %2").arg(plan.videoEncoder, swPlan.videoEncoder));
                    startTranscode(file, info, swPlan);
                    return;
                }
            }
            fail(tr("ffmpeg failed: %1").arg(m_stderrTail.isEmpty() ? tr("exit code %1").arg(code)
                                                                      : m_stderrTail.last()));
            return;
        }

        if (!m_options.keepOriginal || file != finalPath) {
            if (!m_options.keepOriginal)
                QFile::remove(file);
            QFile::remove(finalPath);
        }
        if (!QFile::rename(m_tempFile, finalPath)) {
            fail(tr("cannot write %1").arg(finalPath));
            return;
        }
        m_tempFile.clear();
        log(tr("Saved: %1").arg(finalPath));
        m_outputs << finalPath;
        ++m_fileIndex;
        processNextFile();
    });

    m_progress = duration > 0 ? 0 : -1;
    setState(State::Converting, detail);
    p->start(m_tools.ffmpeg, args);
}

}  // namespace vidops
