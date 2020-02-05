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


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>

#include "xlsxdocument.h"
#include "xlsxchartsheet.h"
#include "xlsxcellrange.h"
#include "xlsxchart.h"
#include "xlsxrichstring.h"
#include "xlsxworkbook.h"
using namespace QXlsx;

#include "port.h"
#include "PWCL_test\com.h"

#define i_kc            0
#define i_tauI          1
#define i_tauD          2
#define i_tauF          3
#define i_positionForm  4
#define i_filterAll     5
#define i_pOnNominal    6
#define i_setPoint      7
#define i_percentOn     8
#define i_fanSpeed      9
#define i_temperature   10
#define i_tempFiltered  11
#define i_time          12
#define i_inputVar      13
#define i_avg_err       14
#define i_score         15


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    void showRequest(const QString & req);
    bool disonnectedPopUpWindow();
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void response(const QString &response);

private slots:

    void on_portComboBox_activated(int index);
    void on_setButton_clicked();
    void on_posFormCheckBox_stateChanged(int arg1);
    void on_filterAllCheckBox_stateChanged(int arg1);

    void on_actionExport_Excel_File_triggered();

    void on_auto_fit_CheckBox_stateChanged(int arg1);

    void on_zoom_xaxis_checkBox_stateChanged(int arg1);

    void on_zoomy_checkBox_stateChanged(int arg1);

private:
    COM com;


    Ui::MainWindow *ui;

    int timerId;
    void timerEvent(QTimerEvent *event);
    void closeEvent(QCloseEvent* event);
    PORT port;
    bool validConnection;

    QString excelFileName;
    QXlsx::Document xldoc;
    QFile csvdoc;
    QMediaPlayer* player;

    float nominalPercentOn = 0;

protected:
    bool event(QEvent *event);

};

#endif // MAINWINDOW_H
