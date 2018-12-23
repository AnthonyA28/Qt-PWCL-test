#include "port.h"

QT_USE_NAMESPACE

/**
 * Constructor
 */

PORT::PORT(QObject *parent)
    : QThread(parent), quit(false)
{
    qDebug() << " Calling constructor of PORT \n";
    this->isConnected = false;
    this->quit = false;
    this->response = "";
}

/**
 * Destructor
 */

PORT::~PORT()
{
    qDebug() << " Calling destructor of PORT \n";
    mutex.lock();
    this->response = "   !!!!";
    msleep(1000);   //todo: develop solution to ensure the port recieves '!'
    quit = true;
    mutex.unlock();
    wait();
}


void PORT::openPort(const QSerialPortInfo& portInfo_)
{
    QMutexLocker locker(&this->mutex); // todo: see if this is necessary
    this->portInfo = portInfo_;
    qDebug() << " calling openPort of PORT \n";
    qDebug() << "Name: " << this->portInfo.portName() <<"\n";
    qDebug() << "Description: " << this->portInfo.description() <<"\n";
    qDebug() << "Manufacturer: " << this->portInfo.manufacturer() <<"\n";
    start(); // start the thread
}

bool PORT::L_isConnected()
{
    this->mutex.lock();
    bool isConected_ = this->isConnected;
    this->mutex.unlock();
    return isConected_;
}

void PORT::L_processResponse(const QString &response_){
    qDebug() << "Processing response: " << response_;
    this->mutex.lock();
    this->response = response_; // set the input argument to the response
        // if this response is not "" it will be sent to the port and then set to ""
    this->mutex.unlock();
}

void PORT::run()
{

    QSerialPort serial;  // this MUST be created in this threa
    int waitTimeout = 500;  // this seems to prevent the program from using way to much CPU

    this->mutex.lock();
    serial.setPort(this->portInfo);
    this->mutex.unlock();
    if( serial.open(QIODevice::ReadWrite))
    {
        serial.setBaudRate(9600);
        serial.setDataBits(QSerialPort::Data8);
        serial.setBreakEnabled(false);
        serial.setFlowControl(QSerialPort::HardwareControl);
        serial.setParity(QSerialPort::NoParity);
        serial.setStopBits(QSerialPort::OneStop);
        serial.clear();
        serial.clearError();
        serial.setDataTerminalReady(true);  // is required to signal setup() in the arduino
//        serial.setRequestToSend(true);    // is required to signal setup() in the arduino maybe not
        // Successfully opened the port
        qDebug() << " succssfully opened the port \n";

        this->mutex.lock();
        this->isConnected = true;
        this->mutex.unlock();

        while(!quit)
        {
            this->mutex.lock();
            if( this->response != "" )
            {
                if( serial.write(this->response.toUtf8()) )
                {
                    qDebug() << " sent to port: " << this->response;
                } else {
                    qDebug() << " Failed to write to port: " << this->response;
                }
                this->response = "";
            } // else { no need to do anything because string is empty }
            this->mutex.unlock();

            /* Done sending data to the port
               Now we can read any data from the port*/

            if (serial.waitForReadyRead(waitTimeout)) // todo: determine if this is necessary
            {   // if it is necessary add a comment why
                if( serial.canReadLine())
                {
                    char buf[1024];
                    if ( serial.readLine(buf, sizeof(buf)) != -1 )
                    {
                        qDebug() << "emitting request: " << buf << "\n";
                        const QString request = QString::fromStdString(buf);
                        emit this->request(request);
                    }
                    else { qDebug() << " Failed to read line\n";}
                }
                else { /*qDebug() << " cannot read line\n";*/}
            }
            else {/* qDebug() << " read ready timed out \n";*/}
        }

    } else { qDebug() << " Failed to open the port \n";}
}


