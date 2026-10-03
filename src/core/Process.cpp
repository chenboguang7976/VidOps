#include "core/Process.h"

#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

namespace vidops {

QProcess *createToolProcess(QObject *parent)
{
    auto *p = new QProcess(parent);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    p->setProcessEnvironment(env);
#ifdef Q_OS_UNIX
    p->setChildProcessModifier([] { ::setpgid(0, 0); });
#endif
    return p;
}

void terminateProcessTree(QProcess *process)
{
    if (!process || process->state() == QProcess::NotRunning)
        return;
    const qint64 pid = process->processId();
#if defined(Q_OS_WIN)
    // yt-dlp.exe is a PyInstaller bootloader with a child interpreter, which
    // in turn may run ffmpeg: kill the whole tree.
    QProcess::startDetached(QStringLiteral("taskkill"),
                            {QStringLiteral("/PID"), QString::number(pid),
                             QStringLiteral("/T"), QStringLiteral("/F")});
    QPointer<QProcess> guard(process);
    QTimer::singleShot(3000, process, [guard] {
        if (guard && guard->state() != QProcess::NotRunning)
            guard->kill();
    });
#elif defined(Q_OS_UNIX)
    if (pid > 0)
        ::kill(-static_cast<pid_t>(pid), SIGTERM);
    QPointer<QProcess> guard(process);
    QTimer::singleShot(3000, process, [guard, pid] {
        if (guard && guard->state() != QProcess::NotRunning) {
            ::kill(-static_cast<pid_t>(pid), SIGKILL);
            guard->kill();
        }
    });
#else
    process->kill();
#endif
}

}  // namespace vidops
