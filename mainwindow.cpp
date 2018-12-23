#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTextCursor>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    this->timerId = startTimer(1000);  // timer is used to repeatedly check for port data until we are connected
    this->inputs = std::vector<float>(this->numInputs); // initialize the input vector to hold the input values from the port

    connect(&port, &PORT::request, this, &MainWindow::showRequest);     // when the port recieves data it will emit PORT::request thus calling MainWindow::showRequest
    connect(this, &MainWindow::response, &port, &PORT::L_processResponse);  // whn the set button is clicked, it will emit MainWindow::response thus calling PORT::L_processResponse

    ui->outputTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);   // have the table resize with the window

    // need to ensure the files are opened in the correct directory
    QString execDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(execDir);
    QDir newDir = QDir::current();
    newDir.cdUp();  // go one directory up so we dont place our output next to the dependencies
    QDir::setCurrent(newDir.path());

    this->excelFileName = "excelFile.xlsx";
    this->csvFileName   = "data.csv";

    // ensure the excel file does not already exit, if it does we will delete it
    if( QFile::exists(this->excelFileName) )
    {
        QFile oldFile(this->excelFileName);
        oldFile.remove();
    }

    /* give the excel file column headers */
    this->xldoc.write( 1 , 1, "Time");
    this->xldoc.write( 1 , 2, "Percent On");
    this->xldoc.write( 1 , 3, "Temperature");
    this->xldoc.write( 1 , 4, "Filtered Temperature");
    this->xldoc.write( 1 , 5, "Set Point");

    // open the csv file and give it a header
    this->csvdoc.setFileName(this->csvFileName);
    if( this->csvdoc.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text) )
    {
        QTextStream stream(&this->csvdoc);
        stream << "Time, perent on, Temp, Temp filtered, Set Point\n";
    }
    else
    {
        /* todo: figure out how to handle this #p2 */
        qDebug() << " Failed to open data.csv \n";
    }

}



MainWindow::~MainWindow()
{
    if( this->port.L_isConnected())
    {
        // ensure a backupfile directory exists, create one if it doesnt
        QDir backupDir("backupFiles");
        if( !backupDir.exists() )
             backupDir.mkpath(".");
        QDir::setCurrent("backupFiles");

        QDateTime currentTime(QDateTime::currentDateTime());
        QString dateStr = currentTime.toString("d-MMM--h-m-s-A");  // create a date title for the backup file
        dateStr.append(".csv"); // the 's' cant be used when formatting the time string
        this->csvdoc.copy(dateStr); // save a backup file of the csv file
    }
    this->csvdoc.close();


    delete ui;
}

void MainWindow::showRequest(const QString &req)
{
    if(this->deserializeArray(req.toStdString().c_str(), this->numInputs, this->inputs))
    {
        ui->outputTable->insertRow(ui->outputTable->rowCount()); // create a new row

        // todo: fix the condition here inputs size is larger than the amount of columns

        QString tmpStr;


        /* add a string of each value into each column at the last row in the outputTable */
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 0, new QTableWidgetItem(QString::number(inputs[i_time])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 1, new QTableWidgetItem(QString::number(inputs[i_percentOn])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 2, new QTableWidgetItem(QString::number(inputs[i_temperature])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 3, new QTableWidgetItem(QString::number(inputs[i_tempFiltered])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 4, new QTableWidgetItem(QString::number(inputs[i_setPoint])));
        ui->outputTable->scrollToBottom();   // scroll to the bottom to ensure the last value is visible

        /* add each value into the excel file  */
        this->xldoc.write(ui->outputTable->rowCount(), 1, inputs[i_time]);
        this->xldoc.write(ui->outputTable->rowCount(), 2, inputs[i_percentOn]);
        this->xldoc.write(ui->outputTable->rowCount(), 3, inputs[i_temperature]);
        this->xldoc.write(ui->outputTable->rowCount(), 4, inputs[i_tempFiltered]);
        this->xldoc.write(ui->outputTable->rowCount(), 5, inputs[i_setPoint]);
        this->xldoc.saveAs(this->excelFileName); // save the doc in case we crash

        // for writing to csv file
        char file_output_buffer[200]   = ""; // create a string to be sent to the file
        snprintf(file_output_buffer, sizeof(file_output_buffer),"%6.2f,%6.2f,%6.2f,%6.2f,%6.2f\n",
            inputs[i_time], inputs[i_percentOn], inputs[i_temperature], inputs[i_tempFiltered], inputs[i_setPoint]);
        QTextStream stream(&this->csvdoc);
        stream << file_output_buffer;
        stream.flush();
    }
    else
    {
        qDebug() << "ERROR Failed to deserialize array \n";
    }
}

bool MainWindow::deserializeArray(const char* const input, unsigned int output_size,  std::vector<float> &output)
{
    //todo: make this better #p2
    const char*  p = input;
    unsigned int num_commas     = 0;
    unsigned int num_brackets   = 0;
    unsigned int num_values     = 1;
    while (*p) {
      if (*p == '[') { num_brackets++;
      } else if ( *p == ']' ) {num_brackets++;
      } else if ( *p == ',' ) {num_commas++; num_values++;
      } p++;
    }
    if (num_brackets != 2) {
        qDebug() << "(M) Parse error, not valid array\n";
        return false;
    }
    if (num_values != output_size) {
        qDebug() << "(M) Parse error, input size incorrect\n";
        return false;
    }
   char* pEnd;
   p = input + 1;
//    input ++;
   for ( unsigned int i = 0; i < output_size; i ++ ){
       if ( p[0] != '_'){
           output[i] = strtof(p, &pEnd); // strtof can returns nan when parsing nans,
           // strod returns 0 when parsing nans
           pEnd ++;
           p = pEnd;
       } else {
           do{
               p++;
           } while (*p != ',');
           p++;
       }
   }
   p = input;
   return true;
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    if( !port.L_isConnected() )
    {
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        if(  ui->portComboBox->count() != portList.size() )
        {
            ui->portComboBox->clear();
            for(int i = 0; i < portList.size(); i ++)
            {
               ui->portComboBox->addItem(portList.at(i).portName());
            }
        }
    }
    else
    {
        killTimer(this->timerId); // no reason for the timer anymore
        if( ui->setButton->text() != "Set")
        {
            ui->setButton->setText("Set");
        }
    }

}

void MainWindow::on_portComboBox_activated(int index)
{
    if ( !port.L_isConnected() )
    {  // the port is not conneted yet so we should connect
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        if(portList.size() != 0)
        {
            port.openPort(portList.at(index));
        }
    }
}

void MainWindow::on_setButton_clicked()
{
    // send data to port now
    if ( port.L_isConnected() )
    {   // we are connected so we can send the data in the textbox
        bool isNumerical = false;
        QString pOnStr = ui->percentOnInput->text();   // get string from perent on textbox
        float pOn = pOnStr.toFloat(&isNumerical);    // convert to a float value
        if( !isNumerical )
        {
            // the user input is not a valid number
            QMessageBox msgBox;
            msgBox.setText("The percent on value is not numerical");
            msgBox.exec();
            ui->percentOnInput->clear();
        }
        else if ( pOn > 100 || pOn < 0 ) // ensure the not out of range of a reasonable percent on value
        {
            QMessageBox msgBox;
            msgBox.setText("The percent on value is out of range");
            msgBox.exec();
            ui->percentOnInput->clear();
        }
        else
        {
            // its okay
            pOnStr.prepend("[");
            pOnStr.append("]");
            emit this->response(pOnStr);
        }
    }
    else
    {  // as if the user clicked the first option in the combobox
        emit this->on_portComboBox_activated(0);
    }
}
