#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("VidOps"));
    QApplication::setApplicationName(QStringLiteral("VidOps"));
    QApplication::setApplicationVersion(QStringLiteral(VIDOPS_VERSION));

    QApplication::setWindowIcon(QIcon(QStringLiteral(":/vidops.png")));
    QGuiApplication::setDesktopFileName(QStringLiteral("vidops"));

    qRegisterMetaType<vidops::ToolSet>("vidops::ToolSet");

    vidops::MainWindow window;
    window.show();
    return app.exec();
}
