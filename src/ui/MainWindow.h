#pragma once

#include "core/AppSettings.h"
#include "core/DownloadManager.h"
#include "core/Tools.h"

#include <QHash>
#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTableWidgetItem;

namespace vidops {

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum Column { ColTitle, ColFormat, ColStatus, ColProgress, ColSpeed, ColEta, ColCount };

    void buildUi();
    void buildMenus();
    void detectTools();
    void onToolsDetected(const ToolSet &tools);
    void refreshEncoderCombo();
    void updateOptionWidgets();
    void storeUiOptions();
    DownloadOptions currentOptions() const;

    void addUrlsFromInput();
    void pasteFromClipboard();
    void browseOutputDir();
    void openOutputDir();
    void openSettings();
    void updateYtDlp();
    void showAbout();
    void showContextMenu(const QPoint &pos);

    void onJobAdded(DownloadJob *job);
    void onJobChanged(DownloadJob *job);
    void onJobRemoved(DownloadJob *job);
    QList<DownloadJob *> selectedJobs() const;
    void appendLog(const QString &line);
    void updateSummary();

    AppSettings m_settings;
    ToolSet m_tools;
    DownloadManager *m_manager;
    ToolDetector *m_detector;
    QHash<DownloadJob *, QTableWidgetItem *> m_rows;

    QPlainTextEdit *m_urlEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_qualityCombo = nullptr;
    QComboBox *m_encoderCombo = nullptr;
    QCheckBox *m_reencodeCheck = nullptr;
    QCheckBox *m_playlistCheck = nullptr;
    QLineEdit *m_dirEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_toolsLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
};

}  // namespace vidops
