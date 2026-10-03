#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace vidops {

SettingsDialog::SettingsDialog(const AppSettings &settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(560);
    const DownloadOptions &o = settings.options;

    // Tools
    auto *toolsBox = new QGroupBox(tr("Tools (leave empty to auto-detect)"));
    auto *toolsForm = new QFormLayout(toolsBox);
    m_ytdlp = pathRow(settings.ytdlpPath, tr("auto"));
    m_ffmpeg = pathRow(settings.ffmpegPath, tr("auto"));
    m_ffprobe = pathRow(settings.ffprobePath, tr("auto"));
    toolsForm->addRow(QStringLiteral("yt-dlp:"), m_ytdlp->parentWidget());
    toolsForm->addRow(QStringLiteral("ffmpeg:"), m_ffmpeg->parentWidget());
    toolsForm->addRow(QStringLiteral("ffprobe:"), m_ffprobe->parentWidget());

    // Download
    auto *dlBox = new QGroupBox(tr("Download"));
    auto *dlForm = new QFormLayout(dlBox);
    m_cookiesBrowser = new QComboBox;
    m_cookiesBrowser->addItem(tr("None"), QString());
    for (const char *b : {"chrome", "firefox", "edge", "safari", "brave", "opera", "vivaldi", "chromium"})
        m_cookiesBrowser->addItem(QString::fromLatin1(b), QString::fromLatin1(b));
    m_cookiesBrowser->setCurrentIndex(qMax(0, m_cookiesBrowser->findData(o.cookiesBrowser)));
    m_cookiesBrowser->setToolTip(tr("Use the login cookies of this browser — needed for private, "
                                    "age-restricted or members-only videos."));
    m_cookiesFile = pathRow(o.cookiesFile, tr("cookies.txt (optional, overrides browser)"));
    m_template = new QLineEdit(o.filenameTemplate);
    m_template->setPlaceholderText(defaultFilenameTemplate());
    m_template->setToolTip(tr("yt-dlp output template, e.g. %(uploader)s/%(title)s.%(ext)s"));
    m_concurrent = new QSpinBox;
    m_concurrent->setRange(1, 8);
    m_concurrent->setValue(settings.maxConcurrent);
    dlForm->addRow(tr("Cookies from browser:"), m_cookiesBrowser);
    dlForm->addRow(tr("Cookies file:"), m_cookiesFile->parentWidget());
    dlForm->addRow(tr("File name template:"), m_template);
    dlForm->addRow(tr("Parallel downloads:"), m_concurrent);

    // Encoding
    auto *encBox = new QGroupBox(tr("Conversion"));
    auto *encForm = new QFormLayout(encBox);
    m_speed = new QComboBox;
    for (SpeedPreset s : {SpeedPreset::Fast, SpeedPreset::Balanced, SpeedPreset::Quality})
        m_speed->addItem(speedLabel(s), static_cast<int>(s));
    m_speed->setCurrentIndex(m_speed->findData(static_cast<int>(o.speed)));
    m_quality = new QSpinBox;
    m_quality->setRange(-1, 63);
    m_quality->setSpecialValueText(tr("Auto (H.264 23 · H.265 28 · AV1 32)"));
    m_quality->setValue(o.quality);
    m_quality->setToolTip(tr("CRF / constant quality. Lower = better quality, bigger file."));
    m_audioBitrate = new QSpinBox;
    m_audioBitrate->setRange(64, 512);
    m_audioBitrate->setSingleStep(32);
    m_audioBitrate->setSuffix(QStringLiteral(" kbps"));
    m_audioBitrate->setValue(o.audioBitrateKbps);
    m_keepOriginal = new QCheckBox(tr("Keep the original file after converting"));
    m_keepOriginal->setChecked(o.keepOriginal);
    encForm->addRow(tr("Encoder speed:"), m_speed);
    encForm->addRow(tr("Quality (CRF):"), m_quality);
    encForm->addRow(tr("AAC bitrate:"), m_audioBitrate);
    encForm->addRow(QString(), m_keepOriginal);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(toolsBox);
    layout->addWidget(dlBox);
    layout->addWidget(encBox);
    layout->addWidget(buttons);
}

QLineEdit *SettingsDialog::pathRow(const QString &value, const QString &placeholder, bool directory)
{
    auto *row = new QWidget(this);
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    auto *edit = new QLineEdit(value, row);
    edit->setPlaceholderText(placeholder);
    edit->setClearButtonEnabled(true);
    auto *browse = new QPushButton(tr("Browse…"), row);
    connect(browse, &QPushButton::clicked, this, [this, edit, directory] {
        const QString path = directory
            ? QFileDialog::getExistingDirectory(this, QString(), edit->text())
            : QFileDialog::getOpenFileName(this, QString(), edit->text());
        if (!path.isEmpty())
            edit->setText(path);
    });
    h->addWidget(edit, 1);
    h->addWidget(browse);
    return edit;
}

AppSettings SettingsDialog::settings() const
{
    AppSettings s = m_settings;
    s.ytdlpPath = m_ytdlp->text().trimmed();
    s.ffmpegPath = m_ffmpeg->text().trimmed();
    s.ffprobePath = m_ffprobe->text().trimmed();
    s.maxConcurrent = m_concurrent->value();
    DownloadOptions &o = s.options;
    o.cookiesBrowser = m_cookiesBrowser->currentData().toString();
    o.cookiesFile = m_cookiesFile->text().trimmed();
    o.filenameTemplate = m_template->text().trimmed().isEmpty() ? defaultFilenameTemplate()
                                                                : m_template->text().trimmed();
    o.speed = static_cast<SpeedPreset>(m_speed->currentData().toInt());
    o.quality = m_quality->value();
    o.audioBitrateKbps = m_audioBitrate->value();
    o.keepOriginal = m_keepOriginal->isChecked();
    return s;
}

}  // namespace vidops
