#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Dominator"));
    QCoreApplication::setApplicationName(QStringLiteral("Dominator"));
    MainWindow w;
    w.show();
    return QApplication::exec();
}
