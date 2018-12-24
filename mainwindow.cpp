#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTextCursor>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    this->timerId = startTimer(250);  // timer is used to repeatedly check for port data until we are connected
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
        stream << "Time, percent on, Temp, Temp filtered, Set Point\n";
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

        /* add a string of each value into each column at the last row in the outputTable */
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 0, new QTableWidgetItem(QString::number(inputs[i_time])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 1, new QTableWidgetItem(QString::number(inputs[i_percentOn])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 2, new QTableWidgetItem(QString::number(inputs[i_temperature])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 3, new QTableWidgetItem(QString::number(inputs[i_tempFiltered])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 4, new QTableWidgetItem(QString::number(inputs[i_setPoint])));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 5, new QTableWidgetItem(QString::number(inputs[i_fanSpeed])));
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


        // Show the current values from the port in the current parameters area
        ui->kcLabel->setNum(inputs[i_kc]);
        ui->tauiLabel->setNum(inputs[i_tauI]);
        ui->taudLabel->setNum(inputs[i_tauD]);
        ui->taufLabel->setNum(inputs[i_tauF]);
        ui->avgerrLabel->setNum(inputs[i_avg_err]);
        ui->inputVarLabel->setNum(inputs[i_input_var]);
        ui->scoreLabel->setNum(inputs[i_score]);


      emit this->on_setButton_clicked();
    }
    else
        qDebug() << "ERROR Failed to deserialize array \n";
}



void MainWindow::on_setButton_clicked()
{
    // send data to port now
    if ( port.L_isConnected() )
    {   // we are connected so we can send the data in the textbox

        bool valuesUpdated = false;
        QString response;

        /*
         * Lambda expression used to automate filling the output array from input in the textboxes
         */
        auto fillArrayAtNextIndex = [&response, &valuesUpdated] (double oldVal, QString name, QLineEdit* textBox)
        {
            double eps = 0.01;
            QString valStr = textBox->text();
            bool isNumerical;
            float val = valStr.toFloat(&isNumerical);    // convert to a float value
            if( !valStr.isEmpty() )
            {
                if( !isNumerical )
                {
                    QMessageBox msgBox;
                    msgBox.setText(name + " is not valid" );
                    msgBox.exec();
                    textBox->clear();
                    response.append("_");
                    return;
                }
                else{   // its okay.. lets see if it is different than last measured value
                    if( qFabs(val - oldVal) > eps)
                    {
                        response.append(valStr);
                        valuesUpdated = true;
                    } else
                        response.append("_");
                    return;
                }
            }
            response.append("_");
            return;
        };

            /* use the fillArrNextIndex to place the value from the text box
               into the array if it is different than the last recieved value,
               otherwise it will send an underscore signifying not to change the val
            */
        response = "[";
        fillArrayAtNextIndex(inputs[i_kc],   "Kc", ui->kcTextBox);     response.append(",");
        fillArrayAtNextIndex(inputs[i_tauI], "TauI", ui->tauiTextBox); response.append(",");
        fillArrayAtNextIndex(inputs[i_tauD], "tauD", ui->taudTextBox); response.append(",");
        fillArrayAtNextIndex(inputs[i_tauF], "TauF", ui->taufTextBox); response.append(",");

            /* If checkboxxes reflect differen modes than last read
                when unchecked tha values should be zero
                but they are floats so its safer to check size compared to 0.1
            */

        if ( qFabs(inputs[i_filterAll]) < 0.1  && ui->filterAllCheckBox->isChecked() )
        {
            qDebug() << " diffs filter : old: " << inputs[i_filterAll] << "  and checked \n";
            valuesUpdated = true;
        }
        if ( qFabs(inputs[i_filterAll]) > 0.1  && !ui->filterAllCheckBox->isChecked() )
        {
            qDebug() << " diffs filter : old: " << inputs[i_filterAll] << "  and checked \n";
            valuesUpdated = true;
        }
        if ( qFabs(inputs[i_positionForm]) < 0.1  && ui->posFormCheckBox->isChecked() )
        {
            qDebug() << " diffs form : old: " << inputs[i_positionForm] << "  and checked \n";
            valuesUpdated = true;
        }
        if ( qFabs(inputs[i_positionForm]) > 0.1  && !ui->posFormCheckBox->isChecked() )
        {
            qDebug() << " diffs form : old: " << inputs[i_positionForm] << "  and checked \n";
            valuesUpdated = true;
        }
        response.append( ui->posFormCheckBox->isChecked()   ? "1," : "0," );
        response.append( ui->filterAllCheckBox->isChecked() ? "1]" : "0]" );

        if ( valuesUpdated  )  // this must be checked after calling fillArrayAtNextIndex because that checked if values are updated
            emit this->response(response);
    }
    else
    {  // if we arent connect then emit a signal as if the user clicked the first option in the combobox
        emit this->on_portComboBox_activated(0);
    }

}


void MainWindow::timerEvent(QTimerEvent *event)
{
    /**
      Check if any ports are available and if the port is connected, if its not, then attempt to connect
     */

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
        if( ui->setButton->text() != "Set")   // change connect button to set button
        {
            ui->setButton->setText("Connected");
            ui->setButton->setEnabled(false);
            ui->portComboBox->setEnabled(false);
        }
    }

}


void MainWindow::on_portComboBox_activated(int index)
{
    if ( !port.L_isConnected() )
    {
        // the port is not conneted yet so we should connect
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        if(portList.size() != 0)
        {
            port.openPort(portList.at(index));
        }
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



void MainWindow::on_posFormCheckBox_stateChanged(int arg1)
{
    qDebug() << " checko\n";
    emit on_setButton_clicked();
}

void MainWindow::on_filterAllCheckBox_stateChanged(int arg1)
{
    emit on_setButton_clicked();
}
