#include "core/DownloadManager.h"

namespace vidops {

DownloadJob *DownloadManager::add(const QString &url, const DownloadOptions &options)
{
    auto *job = new DownloadJob(m_nextId++, url, options, this);
    connect(job, &DownloadJob::changed, this, [this, job] { emit jobChanged(job); });
    connect(job, &DownloadJob::logMessage, this, &DownloadManager::logMessage);
    connect(job, &DownloadJob::finished, this, [this] {
        schedule();
        if (activeCount() == 0)
            emit queueIdle();
    }, Qt::QueuedConnection);
    m_jobs << job;
    emit jobAdded(job);
    schedule();
    return job;
}

void DownloadManager::retry(DownloadJob *job)
{
    if (!job || !job->isFinished())
        return;
    job->resetForRetry();
    schedule();
}

void DownloadManager::cancel(DownloadJob *job)
{
    if (job)
        job->cancel();
}

void DownloadManager::cancelAll()
{
    for (DownloadJob *job : std::as_const(m_jobs))
        job->cancel();
}

void DownloadManager::remove(DownloadJob *job)
{
    if (!job || job->isActive())
        return;
    m_jobs.removeOne(job);
    emit jobRemoved(job);
    job->deleteLater();
}

void DownloadManager::setTools(const ToolSet &tools)
{
    m_tools = tools;
    m_toolsReady = true;
    schedule();
}

void DownloadManager::setMaxConcurrent(int n)
{
    m_maxConcurrent = qBound(1, n, 8);
    schedule();
}

int DownloadManager::activeCount() const
{
    int n = 0;
    for (const DownloadJob *job : m_jobs)
        n += job->isActive() ? 1 : 0;
    return n;
}

void DownloadManager::schedule()
{
    if (!m_toolsReady)
        return;  // jobs wait until tool detection has finished
    int running = activeCount();
    for (DownloadJob *job : std::as_const(m_jobs)) {
        if (running >= m_maxConcurrent)
            break;
        if (job->state() == DownloadJob::State::Queued) {
            job->start(m_tools);
            if (job->isActive())
                ++running;
        }
    }
}

}  // namespace vidops
