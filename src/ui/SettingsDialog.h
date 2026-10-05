#pragma once

#include "core/AppSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace vidops {

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings &settings, QWidget *parent = nullptr);
    AppSettings settings() const;

private:
    QLineEdit *pathRow(const QString &value, const QString &placeholder, bool directory = false);

    AppSettings m_settings;
    QLineEdit *m_ytdlp;
    QLineEdit *m_ffmpeg;
    QLineEdit *m_ffprobe;
    QLineEdit *m_extraArgs;
    QComboBox *m_cookiesBrowser;
    QLineEdit *m_cookiesFile;
    QLineEdit *m_template;
    QSpinBox *m_concurrent;
    QComboBox *m_speed;
    QSpinBox *m_quality;
    QSpinBox *m_audioBitrate;
    QCheckBox *m_keepOriginal;
};

}  // namespace vidops
