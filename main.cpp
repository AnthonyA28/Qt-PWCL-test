/*
Copyright (C) 2019  Anthony Arrowood

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "mainwindow.h"
#include <QApplication>

bool release = false;

FILE *pFile= fopen("log", "w");

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED( type ) Q_UNUSED( context ) // to ignore the unused parameter warning

    QByteArray localMsg = msg.toLocal8Bit();
    fprintf(pFile, "%s", localMsg.constData());
    fflush(pFile);

}

int main(int argc, char *argv[])
{
    if(release) qInstallMessageHandler(myMessageOutput); // Install the handler

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    if(release) fclose (pFile);

    return a.exec();

}
