#include <QApplication>
#include "clockwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("墨水屏桌面闹钟");

    ClockWidget clock;
    clock.setWindowTitle("墨水屏桌面闹钟 · E-Ink Desk Clock");
    clock.show();

    return app.exec();
}
