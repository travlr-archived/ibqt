#include "testibqt.h"
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    TestIbQt tiq;

    QTimer::singleShot(0, &tiq, SLOT(run()));
    return a.exec();
}
