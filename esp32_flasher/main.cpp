#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ESP32 Flasher");
    app.setApplicationVersion("1.0");

    MainWindow window;
    window.setWindowTitle("ESP32 烧录工具 v1.0");
    window.resize(780, 620);
    window.show();

    return app.exec();
}
