#pragma once

#include "core/DownloadJob.h"

#include <QList>
#include <QObject>

namespace vidops {

// Owns the jobs and runs up to maxConcurrent of them at a time.
class DownloadManager : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    DownloadJob *add(const QString &url, const DownloadOptions &options);
    void retry(DownloadJob *job);
    void cancel(DownloadJob *job);
    void cancelAll();
    void remove(DownloadJob *job);

    void setTools(const ToolSet &tools);
    const ToolSet &tools() const { return m_tools; }
    void setMaxConcurrent(int n);
    int activeCount() const;
    const QList<DownloadJob *> &jobs() const { return m_jobs; }

signals:
    void jobAdded(vidops::DownloadJob *job);
    void jobChanged(vidops::DownloadJob *job);
    void jobRemoved(vidops::DownloadJob *job);
    void logMessage(const QString &line);
    void queueIdle();

private:
    void schedule();

    QList<DownloadJob *> m_jobs;
    ToolSet m_tools;
    bool m_toolsReady = false;
    int m_maxConcurrent = 2;
    int m_nextId = 1;
};

}  // namespace vidops
