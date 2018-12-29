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
