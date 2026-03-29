#include <mainwindow.h>

#include <QApplication>
#include <QStatusBar>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowIcon(QIcon(":/Images/OVSLogo.jpg"));
    w.showMaximized();
    return a.exec();
}
