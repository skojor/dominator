#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Dominator"));
    QCoreApplication::setApplicationName(QStringLiteral("Dominator"));
    a.setWindowIcon(QIcon(QStringLiteral(":/icons/dominator.svg")));
    MainWindow w;
    w.show();
    return QApplication::exec();
}
