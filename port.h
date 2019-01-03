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


#ifndef PORT_H
#define PORT_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>
#include <QTime>


/**
 * L_ prefix means the function may lock up in the calling thread
 */
class PORT : public QThread //is derived from QThread
{
    Q_OBJECT
public:
    explicit PORT(QObject *parent = nullptr);
    ~PORT() override;
    void run() override;
    void openPort(const QSerialPortInfo& portInfo_);
    bool L_isConnected();
    void L_processResponse(const QString &response_);
private:
    QSerialPortInfo portInfo;
    QString response;
    QMutex mutex;
    bool isConnected;
    bool quit;

signals:
    bool disconnected();
    void request(const QString &req);

public slots:
};

#endif // PORT_H
