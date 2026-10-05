#pragma once

#include "core/Ffmpeg.h"
#include "core/Options.h"
#include "core/Tools.h"

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace vidops {

// One URL in the queue: runs yt-dlp, then (if needed) ffprobe + ffmpeg on
// every file it produced to reach the requested MP4 codec.
class DownloadJob : public QObject
{
    Q_OBJECT
public:
    enum class State { Queued, Downloading, PostProcessing, Converting, Done, Failed, Cancelled };

    DownloadJob(int id, const QString &url, const DownloadOptions &options, QObject *parent = nullptr);
    ~DownloadJob() override;

    void start(const ToolSet &tools);
    void cancel();
    void resetForRetry();

    int id() const { return m_id; }
    QString url() const { return m_url; }
    const DownloadOptions &options() const { return m_options; }
    State state() const { return m_state; }
    bool isActive() const;
    bool isFinished() const;
    QString title() const { return m_title; }
    QString detail() const { return m_detail; }
    double progress() const { return m_progress; }  // 0..100, -1 = indeterminate
    QString speed() const { return m_speed; }
    QString eta() const { return m_eta; }
    QString error() const { return m_error; }
    QString warning() const { return m_warning; }
    QStringList outputFiles() const { return m_outputs; }
    QString statusText() const;

signals:
    void changed();
    void logMessage(const QString &line);
    void finished();

private:
    void setState(State s, const QString &detail = {});
    void fail(const QString &message);
    void finishCancelled();
    void log(const QString &line);
    QProcess *newProcess();
    void drainLines(QProcess *proc, QByteArray &buffer, bool flush,
                    const std::function<void(const QString &)> &handler);

    void runYtDlp(const QStringList &retryArgs);
    void handleYtDlpLine(const QString &line);
    void onYtDlpFinished(int exitCode);
    void processNextFile();
    void startProbe(const QString &file);
    void startTranscode(const QString &file, const ffmpeg::MediaInfo &info,
                        const ffmpeg::TranscodePlan &plan);
    QString convertedPath(const QString &input) const;

    const int m_id;
    const QString m_url;
    DownloadOptions m_options;
    ToolSet m_tools;

    State m_state = State::Queued;
    QString m_title;
    QString m_detail;
    double m_progress = 0;
    QString m_speed;
    QString m_eta;
    QString m_error;
    QString m_warning;
    QString m_lastYtDlpError;
    QStringList m_downloaded;  // files yt-dlp reported
    QStringList m_outputs;     // final files
    int m_fileIndex = 0;
    int m_retries = 0;
    bool m_hwFallbackUsed = false;
    bool m_cancelRequested = false;

    QProcess *m_proc = nullptr;
    QByteArray m_stdoutBuf;
    QByteArray m_stderrBuf;
    QStringList m_stderrTail;
    QString m_tempFile;
    QString m_pathsFile;
};

}  // namespace vidops
