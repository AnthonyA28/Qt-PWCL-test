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
#include "ui_mainwindow.h"
#include <QTextCursor>



/**
*   Called when the application is first opened.
*   Configures main window, log files, and more..
*/
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);
    // timer is used to repeatedly check for port data until we are connected
    this->timerId = startTimer(250);
    // indicates when we are connected to the port AND the correct arduino program is being run
    this->validConnection = false;
    // initialize the input vector to hold the input values from the port
    this->inputs = std::vector<float>(this->numInputs);
    // have the table resize with the window
    ui->outputTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // disable any user input into the text boxes (until a connection is made)
    ui->kcTextBox->setEnabled(false);
    ui->tauiTextBox->setEnabled(false);
    ui->taudTextBox->setEnabled(false);
    ui->taufTextBox->setEnabled(false);


    /*
    *  Connect functions from the PORT class to functions declared in the MainWIndow class and vice versa.
    */
    connect(&port, &PORT::request, this, &MainWindow::showRequest);     // when the port recieves data it will emit PORT::request thus calling MainWindow::showRequest
    connect(&port, &PORT::disconnected, this, &MainWindow::disonnectedPopUpWindow);
    connect(this, &MainWindow::response, &port, &PORT::L_processResponse);  // when the set button is clicked, it will emit MainWindow::response thus calling PORT::L_processResponse


    /*
    *  Prepare data logging files. One in excel format named this->excelFileName
    *  and two in csv file format named this->csvFileName, another csv file
    *  with titled with the current date will copy the contents of this->csvFileName.
    */
    // Move to the directy above the executable
    QString execDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(execDir);
    QDir newDir = QDir::current();
    newDir.cdUp();
    QDir::setCurrent(newDir.path());

    // create the media player
    this->player = new QMediaPlayer;
    player->setMedia(QUrl::fromLocalFile(execDir+"/alarm.wav"));


    this->excelFileName = "excelFile.xlsx";
    this->csvFileName   = "data.csv";

    if( QFile::exists(this->excelFileName) ){
        QFile oldFile(this->excelFileName);
        oldFile.remove();
    }

    // give the excel file column headers
    this->xldoc.write( 1 , 1, "Time");
    this->xldoc.write( 1 , 2, "Percent On");
    this->xldoc.write( 1 , 3, "Temperature");
    this->xldoc.write( 1 , 4, "Filtered Temperature");
    this->xldoc.write( 1 , 5, "Set Point");

    // open the csv file and give it a header
    this->csvdoc.setFileName(this->csvFileName);
    if (this->csvdoc.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text)){
        QTextStream stream(&this->csvdoc);
        stream << "Time, percent on, Temp, Temp filtered, Set Point\n";
    }
    else{
        qDebug() << " Failed to open data.csv \n";
    }


    /*
    * Set up the live plot on the GUI/
    * The set point must have a special scatterstyle so it doesnt connect the lines
    */
    ui->plot->addGraph();
    ui->plot->graph(0)->setName("Set Point");
    ui->plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor("orange"), 5));
    ui->plot->graph(0)->setPen(QPen(Qt::white)); // so we dont see the line connecting the dots
    ui->plot->graph(0)->setValueAxis(ui->plot->yAxis2);

    ui->plot->addGraph();
    ui->plot->graph(1)->setName("Temperature");
    ui->plot->graph(1)->setPen(QPen(Qt::green));
    ui->plot->graph(1)->setValueAxis(ui->plot->yAxis2);

    ui->plot->addGraph();
    ui->plot->graph(2)->setName("Temperature Filtered");
    ui->plot->graph(2)->setPen(QPen(Qt::blue));
    ui->plot->graph(2)->setValueAxis(ui->plot->yAxis2);

    ui->plot->addGraph();
    ui->plot->graph(3)->setName("Percent Heater on");
    ui->plot->graph(3)->setPen(QPen(QColor("purple"))); // line color for the first graph


    ui->plot->xAxis2->setVisible(true);  // show x ticks at top
    ui->plot->xAxis2->setVisible(false); // dont show labels at top
    ui->plot->yAxis2->setVisible(true);  // right y axis labels
    ui->plot->yAxis2->setTickLabels(true);  // show y ticks on right side for temperature

    ui->plot->yAxis->setLabel("Heater [%]");
    ui->plot->yAxis2->setLabel("Temperature [C]");
    ui->plot->xAxis->setLabel("Time [min]");

    // we dont want a legend
    ui->plot->legend->setVisible(false);

}



/**
*   Called when the window is closed.
*   Creates and saves a backup file of logged data.
*/
MainWindow::~MainWindow()
{
    if (this->port.L_isConnected()) {

        // Ensure we have a specified directory named backup. Create it if not
        QDir backupDir("backupFiles");
        if( !backupDir.exists() )
             backupDir.mkpath(".");
        QDir::setCurrent("backupFiles");

        // Create a backup file titles with the current date and time
        QDateTime currentTime(QDateTime::currentDateTime());
        QString dateStr = currentTime.toString("d-MMM--h-m-A");
        dateStr.append(".csv"); // the 's' cant be used when formatting the time string, so append it
        this->csvdoc.copy(dateStr);
    }
    this->csvdoc.close();

    delete ui;
}



/**
*   Called when data was read from the port.
*   Fills a new row in the output table. Updates the graph, and any parameters shown in the GUI.
*/
void MainWindow::showRequest(const QString &req)
{
    if (req.contains('!')) {
        ui->emergencyMessageLabel->setText(req);
            ui->scoreLabel->setNum(static_cast<double>(inputs[i_score])); //show the score
        if(req.contains("overheat")) {
            player->setVolume(100);
            player->play();
            ui->scoreRankLabel->setText("You have earned the rating of Professional Crash Test Dummy." );
        }
        return;
    }


    if(this->deserializeArray(req.toStdString().c_str(), this->numInputs, this->inputs)){

        if (!this->validConnection) {
            this->validConnection = true;  // String was parsed therefore the correct arduino program is uploaded
            // enable user input because we are connected to the correct arduino program now.
            ui->kcTextBox->setEnabled(true);
            ui->tauiTextBox->setEnabled(true);
            ui->taudTextBox->setEnabled(true);
            ui->taufTextBox->setEnabled(true);
            ui->emergencyMessageLabel->clear();
        }

        double time       = static_cast<double>(inputs[i_time]);
        double percentOn  = static_cast<double>(inputs[i_percentOn]);
        double temp       = static_cast<double>(inputs[i_temperature]);
        double tempFilt   = static_cast<double>(inputs[i_tempFiltered]);
        double setPoint   = static_cast<double>(inputs[i_setPoint]);
        double fanSpeed   = static_cast<double>(inputs[i_fanSpeed]);
        double kc         = static_cast<double>(inputs[i_kc]);
        double tauI       = static_cast<double>(inputs[i_tauI]);
        double tauD       = static_cast<double>(inputs[i_tauD]);
        double tauF       = static_cast<double>(inputs[i_tauF]);
        double score      = static_cast<double>(inputs[i_score]);
        double avg_err    = static_cast<double>(inputs[i_avg_err]);
        double input_var  = static_cast<double>(inputs[i_input_var]);
        bool positionForm = static_cast<bool>(inputs[i_positionForm]);
        bool filterAll    = static_cast<bool>(inputs[i_filterAll]);


        /*
        *  Update the output table with the last parameters read from the port.
        */
        ui->outputTable->insertRow(ui->outputTable->rowCount()); // create a new row

        // add a string of each value into each column at the last row in the outputTable
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 0, new QTableWidgetItem(QString::number( time,'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 1, new QTableWidgetItem(QString::number( percentOn,'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 2, new QTableWidgetItem(QString::number( temp,'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 3, new QTableWidgetItem(QString::number( tempFilt,'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 4, new QTableWidgetItem(QString::number( setPoint,'g',3)));
        ui->outputTable->setItem(ui->outputTable->rowCount()-1, 5, new QTableWidgetItem(QString::number( fanSpeed,'g',3)));
        if (!ui->outputTable->underMouse())
            ui->outputTable->scrollToBottom();   // scroll to the bottom to ensure the last value is visible

        // add each value into the excel file
        this->xldoc.write(ui->outputTable->rowCount(), 1,  time);
        this->xldoc.write(ui->outputTable->rowCount(), 2,  percentOn);
        this->xldoc.write(ui->outputTable->rowCount(), 3,  temp);
        this->xldoc.write(ui->outputTable->rowCount(), 4,  tempFilt);
        this->xldoc.write(ui->outputTable->rowCount(), 5,  setPoint);
        this->xldoc.saveAs(this->excelFileName); // save the doc in case we crash

        /*
        *  Update the csv file with the last data read from the port
        */
        char csvOuput[200]   = "";
        snprintf(csvOuput, sizeof(csvOuput),"%6.2f,%6.2f,%6.2f,%6.2f,%6.2f\n",
             time,  percentOn,  temp,  tempFilt,  setPoint);
        QTextStream stream(&this->csvdoc);
        stream << csvOuput;
        stream.flush();


        /*
        *  Show the current values from the port in the current parameters area
        */
        ui->kcLabel->setNum( kc);
        ui->tauiLabel->setNum( tauI);
        ui->taudLabel->setNum( tauD);
        ui->taufLabel->setNum( tauF);

        double errAndInVarTime = 12.0; // time after which we want to show average error and input variance
        double scoreTime = 29.0; // time after which we show the score
        if ( time > errAndInVarTime){ // only show inputVariance and error agter errAndInVarTime
            ui->avgerrLabel->setNum( avg_err);
            ui->inputVarLabel->setNum( input_var);
        }
        if ( time > scoreTime)  // only show score after scoreTime
            ui->scoreLabel->setNum( score);

        QString ModeString = " ";  // holds a string for current mode ex. "Velocity form, Filtering all terms"
        if (  positionForm  ) ModeString.append("Position Form ");
        else ModeString.append("Velocity Form");
        if (  filterAll ) ModeString.append("\nFiltering all terms");
        ui->modeTextLabel->setText(ModeString);

        /*
          After 29 minutes we show the score
          score > 3.0  Accident waiting to happen.
          3.0  >= score > 1.5  Proud owner of a learners permit.
          1.5  >= score > 0.8  Control Student.
          0.8  >= score        Control Master.
        */
        // check the score to determine what the 'rankString' should be
        // todo: simplify this #p3
        if ( time > 29.0) {
            char rankString[300];
            snprintf(rankString, sizeof(rankString), "Accident waiting to happen\n") ;
            if ( score <= 3.0) {
                if ( score <= 1.5) {
                    if ( score <= 0.8) {
                              snprintf(rankString, sizeof(rankString), "You have achieved the rating of Control Master.\n");
                    } else {  snprintf(rankString, sizeof(rankString), "You have achieved the rating of Control Student.\n") ; }
                } else {      snprintf(rankString, sizeof(rankString), "You have achieved the rating of Proud owner of a learners permit.\n") ; }
            }
            ui->scoreRankLabel->setText(rankString);
        }


        /*
        *  Place the latest values in the graph
        */
        ui->plot->graph(3)->addData( time,  percentOn);
        ui->plot->graph(1)->addData( time,  temp);
        ui->plot->graph(2)->addData( time,  tempFilt);
        ui->plot->graph(0)->addData( time,  setPoint);
        ui->plot->replot( QCustomPlot::rpQueuedReplot );
        ui->plot->rescaleAxes(); // ensure graph fits all data
    }
    else{
        qDebug() << "ERROR Failed to deserialize array \n";
        if (!this->validConnection)
            ui->emergencyMessageLabel->setText("Possible incorrect arduino program uploaded.");
    }
}



/**
*   Called when the user clicks the set button.
*   creates a string from the values in the textboxes and sends it to the port.
*/
void MainWindow::on_setButton_clicked()
{
    if ( port.L_isConnected() ){

        QString response;  // is sent to the port


        /*
        *  Lambda expression used to automate filling the output array from input in the textboxes
        *  Ensures the value inputted in the textBox is within range min and max
        *  returns '_' if the value is no good. When the arduino recieves '_' it wont change the value.
        */
        auto fillArrayAtNextIndex = [&response] ( QString name, QLineEdit* textBox, float min = NAN, float max = NAN)
        {
            QString valStr = textBox->text();   // 'valStr' is a string holding what the user inputted in the texbox
            bool isNumerical = false;
            valStr.remove(' ');  // remove any extra spaces
            if (!valStr.isEmpty()) {
                float val = valStr.toFloat(&isNumerical);    // convert to a float value
                if (!isNumerical){
                    QMessageBox msgBox;
                    msgBox.setText(name + " is not valid" );
                    msgBox.exec();
                    textBox->clear();
                    response.append("_");
                    return;
                } else {
                    // ensure the value is within range
                    if (max != NAN && val > max) {  // max is NAN if it is unconstrained
                        QMessageBox msgBox;
                        msgBox.setText(name + " of " + QString::number(static_cast<double>(val)) + " is over the maximum of " + QString::number(static_cast<double>(max)) );
                        msgBox.exec();
                        textBox->clear();
                        response.append("_");
                        return;
                    }
                if  (min != NAN && val < min){ // min is NAN if it is unconstrained
                        QMessageBox msgBox;
                        msgBox.setText(name + " of " + QString::number(static_cast<double>(val)) + " is below the minimum of " + QString::number(static_cast<double>(min)) );
                        msgBox.exec();
                        textBox->clear();
                        response.append("_");
                        return;
                    }
                    response.append(valStr);
                    return;
                }
            }
            response.append("_");
            return;
        };


        /*
        *  use the 'fillArrNextIndex' to place the value from the text box
        *  into the array if it is different than the last recieved value,
        *  otherwise it will send an underscore signifying not to change the val
        */
        response = "[";
        fillArrayAtNextIndex("Kc", ui->kcTextBox);
        response.append(",");
        fillArrayAtNextIndex("TauI", ui->tauiTextBox, 0);
        response.append(",");
        fillArrayAtNextIndex("tauD", ui->taudTextBox, 0);
        response.append(",");
        fillArrayAtNextIndex("TauF", ui->taufTextBox, 0);
        response.append(",");

        // these two have a different order in the main program but its okay
        // todo: consider fixing that #p2
        response.append( ui->posFormCheckBox->isChecked()   ? "1," : "0," );
        response.append( ui->filterAllCheckBox->isChecked() ? "1]" : "0]" );

        emit this->response(response);
    } else {
        /*
        *  if we arent connect then emit a signal as if the user clicked the first option in the combobox
        *  and therefore a connection will be attempted.
        */
        emit this->on_portComboBox_activated(0);
    }

}



/**
*   Called by the timer ever 250 ms, until the timer is killed.
*   Checks if a connection is active, if no connection is active,
*   search for available ports and open a connection.
*/
void MainWindow::timerEvent(QTimerEvent *event)
{
    if (!port.L_isConnected()) {
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        // todo: check what hapens if the socket changes. This may be an issue if we change the COM before establishing a connection  #p2
        if (ui->portComboBox->count() != portList.size()) {
            ui->portComboBox->clear();
            for (int i = 0; i < portList.size(); i ++) {
               ui->portComboBox->addItem(portList.at(i).portName());
            }
        }
    } else {
        killTimer(this->timerId); // no reason for the timer anymore
        ui->setButton->setText("Set");   // change connect button to set button
    }

}



/**
*   called when the user pressed on the combobox
*   connects to the port selected.
*/
void MainWindow::on_portComboBox_activated(int index)
{
    if (!port.L_isConnected()) {
        // the port is not conneted yet so we should connect
        QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
        if (portList.size() != 0)
            port.openPort(portList.at(index));
    }
}



/**
*   Called after new data is recieved from the port to parse the string recieved.
*   fills 'output' of five 'output_size' with the vlues in 'input' string.
*/
bool MainWindow::deserializeArray(const char* const input, unsigned int output_size,  std::vector<float> &output)
{
    /*
    *   Ensure that the input string has the correct format and number of numbers to be parsed
    */
    const char*  p = input;
    unsigned int num_commas     = 0;
    unsigned int num_brackets   = 0;
    unsigned int num_values     = 0;

    while (*p)
    {
      if (*p == '[') { num_brackets++;
      } else if ( *p == ']' ) {num_brackets++; num_values++;
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


    /*
    *   Correct number of parameters were found,
    *   'output' will not be filled with the values
    */
    char* pEnd;
    p = input + 1;
    for ( unsigned int i = 0; i < output_size; i ++ )
    {

        bool is_a_number = false;
        const char* nc = p; // nc will point to the next comma or the closing bracket
        while(*nc != ',' && *nc != ']' && *nc)
        {
            if ( (int)*nc >= 48 && (int)*nc <= 57 )
                is_a_number = true;
            nc++;
        }
        float num_ = output[i] = strtof(p, &pEnd); // strtof can returns nan when parsing nans,
           // strod returns 0 when parsing nans
        p = pEnd;
        if ( is_a_number || num_ == NAN)
            output[i] = num_;
        while (*p != ',' && *p != ']' && *p)
            p++;
        p++;
   }
   p = input;
   return true;
}


/**
*   Called when the user clicked the position form checkbox
*   emits set_button_clicked so the new parameters will be sent to the port
*/
void MainWindow::on_posFormCheckBox_stateChanged(int arg1)
{
    emit on_setButton_clicked();
}



/**
*   Called when the user clicked the filterAll checkbox
*   emits set_button_clicked so the new parameters will be sent to the port
*/
void MainWindow::on_filterAllCheckBox_stateChanged(int arg1)
{
    emit on_setButton_clicked();
}



/**
*   Called when the port is disconnected
*   Tells the user to manually restart the application
*/
bool MainWindow::disonnectedPopUpWindow()
{
    QMessageBox::critical(this,
                          "Error",
                          "Fatal Error, device disconnected.\n"
                          "Close and restart the application to continue.\n",
                          QMessageBox::Ok
                          );
}


/**
 * called when the user presses enter or return
 * // thank you numbat: https://www.qtcentre.org/threads/26313-MainWindow-Button
 */
bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) {
            emit on_setButton_clicked();
            return true;
        }
    }
    return QMainWindow::event(event);
}
