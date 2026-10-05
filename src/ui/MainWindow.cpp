#include "ui/MainWindow.h"

#include "core/Ffmpeg.h"
#include "ui/ProgressDelegate.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace vidops {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_settings(AppSettings::load()),
      m_manager(new DownloadManager(this)),
      m_detector(new ToolDetector(this))
{
    setWindowTitle(QStringLiteral("VidOps"));
    setAcceptDrops(true);
    buildUi();
    buildMenus();

    m_manager->setMaxConcurrent(m_settings.maxConcurrent);
    connect(m_manager, &DownloadManager::jobAdded, this, &MainWindow::onJobAdded);
    connect(m_manager, &DownloadManager::jobChanged, this, &MainWindow::onJobChanged);
    connect(m_manager, &DownloadManager::jobRemoved, this, &MainWindow::onJobRemoved);
    connect(m_manager, &DownloadManager::logMessage, this, &MainWindow::appendLog);
    connect(m_detector, &ToolDetector::detected, this, &MainWindow::onToolsDetected);

    QSettings s;
    if (!restoreGeometry(s.value(QStringLiteral("ui/geometry")).toByteArray()))
        resize(1000, 680);

    updateSummary();
    detectTools();
}

// ------------------------------------------------------------------ UI setup

void MainWindow::buildUi()
{
    auto *central = new QWidget;
    auto *root = new QVBoxLayout(central);

    // Links
    auto *linkBox = new QGroupBox(tr("Links"));
    auto *linkLayout = new QHBoxLayout(linkBox);
    m_urlEdit = new QPlainTextEdit;
    m_urlEdit->setPlaceholderText(tr("Paste one or more video links (YouTube, TikTok, Facebook, "
                                     "Instagram, X/Twitter, Vimeo…), one per line"));
    m_urlEdit->setMaximumHeight(90);
    m_urlEdit->setTabChangesFocus(true);
    auto *linkButtons = new QVBoxLayout;
    auto *pasteBtn = new QPushButton(tr("Paste"));
    auto *addBtn = new QPushButton(tr("Download"));
    addBtn->setDefault(true);
    addBtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    addBtn->setToolTip(tr("Add the links to the queue (Ctrl+Enter)"));
    linkButtons->addWidget(pasteBtn);
    linkButtons->addWidget(addBtn);
    linkButtons->addStretch();
    linkLayout->addWidget(m_urlEdit, 1);
    linkLayout->addLayout(linkButtons);
    connect(pasteBtn, &QPushButton::clicked, this, &MainWindow::pasteFromClipboard);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addUrlsFromInput);

    // Options
    auto *optBox = new QGroupBox(tr("Output"));
    auto *grid = new QGridLayout(optBox);
    m_formatCombo = new QComboBox;
    for (OutputFormat f : allFormats())
        m_formatCombo->addItem(formatDescription(f), static_cast<int>(f));
    m_formatCombo->setCurrentIndex(qMax(0, m_formatCombo->findData(static_cast<int>(m_settings.options.format))));

    m_qualityCombo = new QComboBox;
    m_qualityCombo->addItem(tr("Best available"), 0);
    for (int h : {2160, 1440, 1080, 720, 480, 360})
        m_qualityCombo->addItem(QStringLiteral("%1p").arg(h), h);
    const int qIdx = m_qualityCombo->findData(m_settings.options.maxHeight);
    m_qualityCombo->setCurrentIndex(qIdx >= 0 ? qIdx : m_qualityCombo->findData(1080));
    m_qualityCombo->setToolTip(tr("Maximum resolution; the best stream up to this height is used."));

    m_encoderCombo = new QComboBox;
    m_encoderCombo->setToolTip(tr("Used only when the video must be converted. Hardware encoders "
                                  "are much faster; VidOps falls back to CPU if one fails."));
    refreshEncoderCombo();

    m_reencodeCheck = new QCheckBox(tr("Always re-encode"));
    m_reencodeCheck->setChecked(m_settings.options.forceReencode);
    m_reencodeCheck->setToolTip(tr("Re-encode even when the source already has the chosen codec."));
    m_playlistCheck = new QCheckBox(tr("Whole playlist / channel"));
    m_playlistCheck->setChecked(m_settings.options.playlist);

    m_dirEdit = new QLineEdit(m_settings.options.outputDir);
    auto *browseBtn = new QPushButton(tr("Browse…"));
    auto *openDirBtn = new QPushButton(tr("Open"));
    connect(browseBtn, &QPushButton::clicked, this, &MainWindow::browseOutputDir);
    connect(openDirBtn, &QPushButton::clicked, this, &MainWindow::openOutputDir);

    grid->addWidget(new QLabel(tr("Format:")), 0, 0);
    grid->addWidget(m_formatCombo, 0, 1);
    grid->addWidget(new QLabel(tr("Quality:")), 0, 2);
    grid->addWidget(m_qualityCombo, 0, 3);
    grid->addWidget(new QLabel(tr("Encoder:")), 1, 0);
    grid->addWidget(m_encoderCombo, 1, 1);
    auto *checks = new QHBoxLayout;
    checks->addWidget(m_reencodeCheck);
    checks->addWidget(m_playlistCheck);
    checks->addStretch();
    grid->addLayout(checks, 1, 2, 1, 2);
    grid->addWidget(new QLabel(tr("Save to:")), 2, 0);
    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(m_dirEdit, 1);
    dirRow->addWidget(browseBtn);
    dirRow->addWidget(openDirBtn);
    grid->addLayout(dirRow, 2, 1, 1, 3);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(3, 1);

    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateOptionWidgets();
        storeUiOptions();
    });
    for (QComboBox *c : {m_qualityCombo, m_encoderCombo})
        connect(c, &QComboBox::currentIndexChanged, this, &MainWindow::storeUiOptions);
    for (QCheckBox *c : {m_reencodeCheck, m_playlistCheck})
        connect(c, &QCheckBox::toggled, this, &MainWindow::storeUiOptions);
    connect(m_dirEdit, &QLineEdit::editingFinished, this, &MainWindow::storeUiOptions);

    // Queue
    m_table = new QTableWidget(0, ColCount);
    m_table->setHorizontalHeaderLabels({tr("Title"), tr("Format"), tr("Status"), tr("Progress"),
                                        tr("Speed"), tr("ETA")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->hide();
    m_table->setWordWrap(false);
    m_table->setItemDelegateForColumn(ColProgress, new ProgressDelegate(m_table));
    QHeaderView *hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    hh->setSectionResizeMode(ColStatus, QHeaderView::Interactive);
    m_table->setColumnWidth(ColFormat, 110);
    m_table->setColumnWidth(ColStatus, 200);
    m_table->setColumnWidth(ColProgress, 140);
    m_table->setColumnWidth(ColSpeed, 100);
    m_table->setColumnWidth(ColEta, 70);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row) {
        auto *job = m_table->item(row, ColTitle)->data(Qt::UserRole).value<DownloadJob *>();
        if (job && !job->outputFiles().isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(job->outputFiles().constFirst()));
    });

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_log->setPlaceholderText(tr("Log"));

    auto *splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(m_table);
    splitter->addWidget(m_log);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    root->addWidget(linkBox);
    root->addWidget(optBox);
    root->addWidget(splitter, 1);
    setCentralWidget(central);

    m_toolsLabel = new QLabel(tr("Detecting tools…"));
    m_summaryLabel = new QLabel;
    statusBar()->addWidget(m_toolsLabel, 1);
    statusBar()->addPermanentWidget(m_summaryLabel);

    updateOptionWidgets();
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("Paste links"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V),
                    this, &MainWindow::pasteFromClipboard);
    file->addAction(tr("Open download folder"), this, &MainWindow::openOutputDir);
    file->addSeparator();
    file->addAction(tr("&Settings…"), QKeySequence::Preferences, this, &MainWindow::openSettings);
    file->addSeparator();
    QAction *quit = file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);
    quit->setMenuRole(QAction::QuitRole);

    QMenu *queue = menuBar()->addMenu(tr("&Queue"));
    queue->addAction(tr("Cancel selected"), this, [this] {
        for (DownloadJob *j : selectedJobs())
            m_manager->cancel(j);
    });
    queue->addAction(tr("Retry selected"), this, [this] {
        for (DownloadJob *j : selectedJobs())
            m_manager->retry(j);
    });
    queue->addAction(tr("Cancel all"), this, [this] { m_manager->cancelAll(); });
    queue->addSeparator();
    queue->addAction(tr("Clear finished"), this, [this] {
        const QList<DownloadJob *> jobs = m_manager->jobs();
        for (DownloadJob *j : jobs) {
            if (j->isFinished())
                m_manager->remove(j);
        }
    });

    QMenu *tools = menuBar()->addMenu(tr("&Tools"));
    tools->addAction(tr("Update yt-dlp"), this, &MainWindow::updateYtDlp);
    tools->addAction(tr("Re-detect tools"), this, &MainWindow::detectTools);
    tools->addAction(tr("Clear log"), this, [this] { m_log->clear(); });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    QAction *about = help->addAction(tr("About VidOps"), this, &MainWindow::showAbout);
    about->setMenuRole(QAction::AboutRole);
}

// ------------------------------------------------------------------ tools

void MainWindow::detectTools()
{
    m_toolsLabel->setText(tr("Detecting tools…"));
    m_detector->detect(m_settings.ytdlpPath, m_settings.ffmpegPath, m_settings.ffprobePath);
}

void MainWindow::onToolsDetected(const ToolSet &tools)
{
    m_tools = tools;
    m_manager->setTools(tools);
    refreshEncoderCombo();

    QStringList parts;
    QStringList missing;
    if (tools.ytdlp.isEmpty())
        missing << QStringLiteral("yt-dlp");
    else
        parts << QStringLiteral("yt-dlp %1").arg(tools.ytdlpVersion.isEmpty() ? tr("(not working)") : tools.ytdlpVersion);
    if (tools.ffmpeg.isEmpty())
        missing << QStringLiteral("ffmpeg");
    else
        parts << QStringLiteral("ffmpeg %1").arg(tools.ffmpegVersion);
    if (tools.ffprobe.isEmpty())
        missing << QStringLiteral("ffprobe");

    QString text = parts.join(QStringLiteral(" · "));
    if (!missing.isEmpty()) {
        text += (text.isEmpty() ? QString() : QStringLiteral(" · "))
              + tr("Missing: %1").arg(missing.join(QStringLiteral(", ")));
        appendLog(tr("Missing tools: %1. Put them next to VidOps, in its tools/ folder, on PATH, "
                     "or set their paths in File → Settings.").arg(missing.join(QStringLiteral(", "))));
    }
    m_toolsLabel->setText(text);
    m_toolsLabel->setToolTip(QStringLiteral("yt-dlp: %1\nffmpeg: %2\nffprobe: %3\ndeno: %4")
                                 .arg(tools.ytdlp, tools.ffmpeg, tools.ffprobe,
                                      tools.deno.isEmpty() ? tr("not found") : tools.deno));
    if (!tools.ytdlp.isEmpty())
        appendLog(tr("Using yt-dlp: %1").arg(tools.ytdlp));
    if (!tools.ffmpeg.isEmpty())
        appendLog(tr("Using ffmpeg: %1").arg(tools.ffmpeg));
    if (!tools.deno.isEmpty())
        appendLog(tr("Using deno: %1").arg(tools.deno));
    else
        appendLog(tr("deno was not found: YouTube may offer fewer formats. "
                     "Put deno next to yt-dlp or install it from https://deno.com"));
}

void MainWindow::refreshEncoderCombo()
{
    const QSignalBlocker blocker(m_encoderCombo);
    const int wanted = static_cast<int>(m_settings.options.encoder);
    m_encoderCombo->clear();
    for (Encoder e : allEncoders()) {
        bool usable = e == Encoder::Software || m_tools.encoders.isEmpty();
        for (OutputFormat f : {OutputFormat::Mp4H264, OutputFormat::Mp4H265, OutputFormat::Mp4AV1}) {
            QString note;
            const QString name = ffmpeg::pickVideoEncoder(f, e, m_tools.encoders, &note);
            usable = usable || (!name.isEmpty() && note.isEmpty());
        }
#ifndef Q_OS_MACOS
        if (e == Encoder::VideoToolbox)
            usable = false;
#endif
        if (usable)
            m_encoderCombo->addItem(encoderLabel(e), static_cast<int>(e));
    }
    m_encoderCombo->setCurrentIndex(qMax(0, m_encoderCombo->findData(wanted)));
}

void MainWindow::updateOptionWidgets()
{
    const auto f = static_cast<OutputFormat>(m_formatCombo->currentData().toInt());
    m_qualityCombo->setEnabled(!isAudioOnly(f));
    m_encoderCombo->setEnabled(convertsVideo(f));
    m_reencodeCheck->setEnabled(convertsVideo(f));
}

void MainWindow::storeUiOptions()
{
    DownloadOptions &o = m_settings.options;
    o.format = static_cast<OutputFormat>(m_formatCombo->currentData().toInt());
    o.maxHeight = m_qualityCombo->currentData().toInt();
    if (m_encoderCombo->count() > 0)
        o.encoder = static_cast<Encoder>(m_encoderCombo->currentData().toInt());
    o.forceReencode = m_reencodeCheck->isChecked();
    o.playlist = m_playlistCheck->isChecked();
    o.outputDir = m_dirEdit->text().trimmed();
    m_settings.save();
}

DownloadOptions MainWindow::currentOptions() const
{
    DownloadOptions o = m_settings.options;
    if (o.outputDir.isEmpty())
        o.outputDir = AppSettings::defaultOutputDir();
    o.ffmpegPath = m_tools.ffmpeg;
    o.extraArgs = QProcess::splitCommand(m_settings.ytdlpExtraArgs);
    return o;
}

// ------------------------------------------------------------------ actions

void MainWindow::addUrlsFromInput()
{
    storeUiOptions();
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    const QStringList tokens = m_urlEdit->toPlainText().split(ws, Qt::SkipEmptyParts);
    QStringList invalid;
    int added = 0;
    const DownloadOptions opts = currentOptions();
    for (const QString &t : tokens) {
        const QUrl url = QUrl::fromUserInput(t);
        if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
            || !url.host().contains(QLatin1Char('.'))) {
            invalid << t;
            continue;
        }
        m_manager->add(url.toString(), opts);
        ++added;
    }
    m_urlEdit->setPlainText(invalid.join(QLatin1Char('\n')));
    if (!invalid.isEmpty())
        statusBar()->showMessage(tr("%n link(s) not recognised and left in the box", nullptr, int(invalid.size())), 6000);
    else if (added > 0)
        statusBar()->showMessage(tr("%n link(s) added", nullptr, added), 4000);
}

void MainWindow::pasteFromClipboard()
{
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    if (text.isEmpty())
        return;
    QString current = m_urlEdit->toPlainText().trimmed();
    if (!current.isEmpty())
        current += QLatin1Char('\n');
    m_urlEdit->setPlainText(current + text);
    m_urlEdit->setFocus();
}

void MainWindow::browseOutputDir()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Save videos to"), m_dirEdit->text());
    if (!dir.isEmpty()) {
        m_dirEdit->setText(QDir::toNativeSeparators(dir));
        storeUiOptions();
    }
}

void MainWindow::openOutputDir()
{
    const QString dir = m_dirEdit->text().trimmed().isEmpty() ? AppSettings::defaultOutputDir()
                                                                : m_dirEdit->text().trimmed();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::openSettings()
{
    storeUiOptions();
    SettingsDialog dlg(m_settings, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const AppSettings old = m_settings;
    m_settings = dlg.settings();
    m_settings.save();
    m_manager->setMaxConcurrent(m_settings.maxConcurrent);
    if (old.ytdlpPath != m_settings.ytdlpPath || old.ffmpegPath != m_settings.ffmpegPath
        || old.ffprobePath != m_settings.ffprobePath)
        detectTools();
}

void MainWindow::updateYtDlp()
{
    if (m_tools.ytdlp.isEmpty()) {
        QMessageBox::warning(this, tr("Update yt-dlp"), tr("yt-dlp was not found."));
        return;
    }
    appendLog(tr("Updating yt-dlp…"));
    runCapture(this, m_tools.ytdlp, {QStringLiteral("-U")}, 180000, [this](bool ok, const QString &out) {
        for (const QString &l : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            appendLog(QStringLiteral("yt-dlp: ") + l.trimmed());
        appendLog(ok ? tr("yt-dlp update finished") : tr("yt-dlp update failed (see log)"));
        detectTools();
    });
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About VidOps"),
        tr("<h3>VidOps %1</h3>"
           "<p>Cross-platform desktop video downloader for YouTube, TikTok, Facebook, "
           "Instagram, X and <a href=\"https://github.com/yt-dlp/yt-dlp/blob/master/supportedsites.md\">"
           "1000+ other sites</a>, with conversion to standard MP4 (H.264, H.265, AV1).</p>"
           "<p>Powered by <a href=\"https://github.com/yt-dlp/yt-dlp\">yt-dlp</a> and "
           "<a href=\"https://ffmpeg.org\">FFmpeg</a>. Licensed under the GNU GPL v3.</p>"
           "<p>Only download content you own or have permission to download, and respect "
           "each platform's terms of service.</p>").arg(QStringLiteral(VIDOPS_VERSION)));
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    const QList<DownloadJob *> jobs = selectedJobs();
    if (jobs.isEmpty())
        return;
    DownloadJob *first = jobs.constFirst();
    QMenu menu(this);
    QAction *openFile = menu.addAction(tr("Open file"));
    openFile->setEnabled(!first->outputFiles().isEmpty());
    QAction *openFolder = menu.addAction(tr("Show in folder"));
    QAction *copyUrl = menu.addAction(tr("Copy link"));
    menu.addSeparator();
    QAction *cancel = menu.addAction(tr("Cancel"));
    QAction *retry = menu.addAction(tr("Retry"));
    QAction *remove = menu.addAction(tr("Remove from list"));

    QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == openFile) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(first->outputFiles().constFirst()));
    } else if (chosen == openFolder) {
        const QString dir = first->outputFiles().isEmpty()
            ? first->options().outputDir
            : QFileInfo(first->outputFiles().constFirst()).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    } else if (chosen == copyUrl) {
        QStringList urls;
        for (DownloadJob *j : jobs)
            urls << j->url();
        QGuiApplication::clipboard()->setText(urls.join(QLatin1Char('\n')));
    } else if (chosen == cancel) {
        for (DownloadJob *j : jobs)
            m_manager->cancel(j);
    } else if (chosen == retry) {
        for (DownloadJob *j : jobs)
            m_manager->retry(j);
    } else if (chosen == remove) {
        for (DownloadJob *j : jobs)
            m_manager->remove(j);
    }
}

// ------------------------------------------------------------------ queue view

void MainWindow::onJobAdded(DownloadJob *job)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *title = new QTableWidgetItem;
    title->setData(Qt::UserRole, QVariant::fromValue(job));
    m_table->setItem(row, ColTitle, title);
    for (int c = ColFormat; c < ColCount; ++c)
        m_table->setItem(row, c, new QTableWidgetItem);
    m_rows.insert(job, title);
    onJobChanged(job);
}

void MainWindow::onJobChanged(DownloadJob *job)
{
    QTableWidgetItem *title = m_rows.value(job);
    if (!title)
        return;
    const int row = title->row();
    title->setText(job->title());
    title->setToolTip(job->url());
    m_table->item(row, ColFormat)->setText(formatLabel(job->options().format));

    QTableWidgetItem *status = m_table->item(row, ColStatus);
    status->setText(job->statusText());
    QString tip = job->statusText();
    if (!job->warning().isEmpty())
        tip += QLatin1Char('\n') + job->warning();
    status->setToolTip(tip);
    status->setForeground(job->state() == DownloadJob::State::Failed ? QBrush(QColor(0xd0, 0x30, 0x30))
                                                                      : QBrush());

    QTableWidgetItem *progress = m_table->item(row, ColProgress);
    const double p = job->progress();
    progress->setData(Qt::UserRole, p < 0 ? -1 : int(p));
    progress->setText(p < 0 ? QStringLiteral("…") : QStringLiteral("%1%").arg(p, 0, 'f', 1));
    m_table->item(row, ColSpeed)->setText(job->speed());
    m_table->item(row, ColEta)->setText(job->eta());
    updateSummary();
}

void MainWindow::onJobRemoved(DownloadJob *job)
{
    if (QTableWidgetItem *title = m_rows.take(job))
        m_table->removeRow(title->row());
    updateSummary();
}

QList<DownloadJob *> MainWindow::selectedJobs() const
{
    QList<DownloadJob *> jobs;
    const QModelIndexList rows = m_table->selectionModel()->selectedRows(ColTitle);
    for (const QModelIndex &idx : rows) {
        if (auto *job = idx.data(Qt::UserRole).value<DownloadJob *>())
            jobs << job;
    }
    return jobs;
}

void MainWindow::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void MainWindow::updateSummary()
{
    int active = 0, queued = 0, done = 0, failed = 0;
    for (const DownloadJob *j : m_manager->jobs()) {
        if (j->isActive()) ++active;
        else if (j->state() == DownloadJob::State::Queued) ++queued;
        else if (j->state() == DownloadJob::State::Done) ++done;
        else if (j->state() == DownloadJob::State::Failed) ++failed;
    }
    m_summaryLabel->setText(tr("Active %1 · Queued %2 · Done %3 · Failed %4")
                                .arg(active).arg(queued).arg(done).arg(failed));
}

// ------------------------------------------------------------------ events

void MainWindow::closeEvent(QCloseEvent *event)
{
    storeUiOptions();
    if (m_manager->activeCount() > 0) {
        const auto answer = QMessageBox::question(
            this, tr("Quit VidOps"),
            tr("%n download(s) still running. Stop them and quit?", nullptr, m_manager->activeCount()));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_manager->cancelAll();
    }
    QSettings().setValue(QStringLiteral("ui/geometry"), saveGeometry());
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList links;
    for (const QUrl &u : event->mimeData()->urls()) {
        if (!u.isLocalFile())
            links << u.toString();
    }
    if (links.isEmpty())
        links << event->mimeData()->text().trimmed();
    QString current = m_urlEdit->toPlainText().trimmed();
    if (!current.isEmpty())
        current += QLatin1Char('\n');
    m_urlEdit->setPlainText(current + links.join(QLatin1Char('\n')));
    event->acceptProposedAction();
}

}  // namespace vidops
