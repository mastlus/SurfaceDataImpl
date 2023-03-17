#include "surfacedata.h"

#include <QtCore>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <Qstring>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QtMath>

#include <iostream>
#include <new>

#include <fstream>

#include <chrono>
#include <thread>

SurfaceData::SurfaceData(QObject *parent)   :
    QThread(parent)
{

}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
// Functions


QStringList readOneLn(QFile &f){

    // Seperate Qstring of one line into QStringList of its values
    QString separator(";");
    QString ln = f.readLine();
    QStringList SplitLn = ln.split(separator);

    return SplitLn;
}

bool filterNewDatapoint(QList<double> *Amp1List, QList<double> *smthdAmp1List, double *aValue){
    // Low pass filter construncted with convolution using a left sided exponential moving avergage filter

    int N = Amp1List->size();
    double newSmthdVal = *aValue * Amp1List->at(N-1) + (1 - *aValue) * smthdAmp1List->rbegin()[0];
    smthdAmp1List->append(newSmthdVal);
    return true;
}

bool saveValsToLists(QList<double> &TimeList, QList<double> &Amp1List, QList<double> &Amp2List, QStringList &SplitLn, int &colNum){

    // Save values of one line into corresponding lists
    bool ok;
    for (int colInd = 0; colInd < colNum; ++colInd) {
        double doubleVal = SplitLn[colInd].toDouble(&ok);

        if (ok == false) {
           return false;
           }

        switch (colInd) {
        case 0:
            TimeList.append(doubleVal/1000);
            break;
        case 1:
            Amp1List.append(doubleVal);
            break;
        case 2:
            Amp2List.append(doubleVal);
            break;
        }
    }
    return true;
}


void SurfaceData::phaseRecognition(double* t, double* t_lastMax, double* dT, int* phase, bool* extrDetectionActive, bool* phaseRecogActive)
{
    // Determine phase
    double dt = *t - *t_lastMax;
    *phase = int(dt/ *dT) + 1;

    if (*phase > 10) {
        if (*phase >= 14){
            *phase = 0;                  // Fallback if t < t_lastMax
            *phaseRecogActive = false;
        }
        else{
            *phase = *phase - 10;       // Fallback if t > 13*dT
        }
    }


    // Old Phase Recognition
//    // Determine phase
//    if (*t < *t_lastMax + *dT){
//        *phase = 1;
//    }
//    else if(*t >= *t_lastMax + *dT * 1 && *t < *t_lastMax + *dT * 2){
//        *phase = 2;
//    }
//    else if(*t >= *t_lastMax + *dT * 2 && *t < *t_lastMax + *dT * 3){
//        *phase = 3;
//    }
//    else if(*t >= *t_lastMax + *dT * 3 && *t < *t_lastMax + *dT * 4){
//        *phase = 4;
//    }
//    else if(*t >= *t_lastMax + *dT * 4 && *t < *t_lastMax + *dT * 5){
//        *phase = 5;
//    }
//    else if(*t >= *t_lastMax + *dT * 5 && *t < *t_lastMax + *dT * 6){
//        *phase = 6;
//    }
//    else if(*t >= *t_lastMax + *dT * 6 && *t < *t_lastMax + *dT * 7){
//        *phase = 7;
//    }
//    else if(*t >= *t_lastMax + *dT * 7 && *t < *t_lastMax + *dT * 8){
//        *phase = 8;
//    }
//    else if(*t >= *t_lastMax + *dT * 8 && *t < *t_lastMax + *dT * 9){
//        *phase = 9;
//    }
//    else if(*t >= *t_lastMax + *dT * 9 && *t < *t_lastMax + *dT * 10){
//        *phase = 10;
//    }
//    else if(*t >= *t_lastMax + *dT * 10 && *t < *t_lastMax + *dT * 11){
//        *phase = 1;
//    }
//    else if(*t >= *t_lastMax + *dT * 11 && *t < *t_lastMax + *dT * 12){
//        *phase = 2;
//    }
//    else if(*t >= *t_lastMax + *dT * 12 && *t < *t_lastMax + *dT * 13){
//        *phase = 3;
//    }
//    else{
//        *phase = 0;                  // Fallback if t > 13*dT or if t < t_lastMax
//        *phaseRecogActive = false;
//    }
}



bool SurfaceData::newMaxDetected(QList<double>* AmpList, bool* maxIsLast, int* lastMinInd, int* lastMaxInd){

    // Check if monotony changed in last 3 datapoints
    bool newMax = false;

    if (*maxIsLast && AmpList->rbegin()[2] < AmpList->rbegin()[1] && AmpList->rbegin()[2] < AmpList->rbegin()[0]){
        *maxIsLast = false;
        *lastMinInd = AmpList->size() - 3;
    }
    else if (!*maxIsLast && AmpList->rbegin()[2] > AmpList->rbegin()[1] && AmpList->rbegin()[2] > AmpList->rbegin()[0]){
        if (abs(AmpList->rbegin()[2]-AmpList->at(*lastMinInd)) > 0.1*abs(AmpList->at(*lastMinInd)-AmpList->at(*lastMaxInd))){
            newMax = true;
        }
        *maxIsLast = true;
    }

    return newMax;
}

bool SurfaceData::checkBreathCycleDur(double* T, double* T_planning, double* T_old, bool* phaseRecogActive) {
    if (*T > 0.5 * *T_planning && *T >= 0.75* *T_old && *T <= 1.25 * *T_old){            // 0.5 because of hysteresis
        *phaseRecogActive = true;
    }
    else {
        *phaseRecogActive = false;
        //qDebug() << "phaseRecogActive = false   " << *T << *T_planning * 0.5 << *T_old;
    }
    return *phaseRecogActive;
}



void SurfaceData::run(){

//    // Display files and directories in current folder
//    QDir dir;
//    foreach(QFileInfo item, dir.entryInfoList() )
//        {
//            if(item.isDir())
//                qDebug() << "Dir: " << item.absoluteFilePath();
//            if(item.isFile())
//                qDebug() << "File: " << item.absoluteFilePath();
//        }

    // Open csv file
    QFile f("SurfaceData/" + this->path + ".csv");

    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << f.errorString();
    }

    // Locate row of headers and determine number of columns
    int colNum = 0;

    QString twoHeaders = { "Time (ms); Pri Amplitude (mm)\r\n" };
    QString threeHeaders = { "Time (ms); Pri Amplitude (mm); Sec Amplitude (mm);\r\n" };

    for (int lnInd = 0; lnInd <= 5; ++lnInd) {
        QString ln1 = f.readLine(0);

        if (lnInd == 5) {
            qDebug() << f.errorString();
        }
        else if (ln1 == twoHeaders) {
            colNum = 2;
            break;
        }
        else if (ln1 == threeHeaders) {
            colNum = 3;
            break;
        }
    }

    // Check if number of columns is permissible
    if (colNum == 0) {
        qDebug() << f.errorString();
    }

    // Iteration index for start of filtering
    int iteration = 0;

    extrDetectionActive = false;
    QMutex mutex;               // Lock lists and columns to avoid acessing same storage twice at the same time


    // Read data line by line
    while (!f.atEnd()){

        iteration = iteration + 1;

        // Read one line
        QStringList SplitLn = readOneLn(f);

        // Stop if stop button was clicked
        // if(this->Stop) break; -> old code

        // Emit signal to update time and ampl values in UI
        emit LnReadingFinished(SplitLn, colNum);

        // Save line in lists (TimeList, Amp1List, Amp2List)
        mutex.lock();
        if (!saveValsToLists(TimeList, Amp1List, Amp2List, SplitLn, colNum)) {
            qDebug() << f.errorString();
        }
        mutex.unlock();


        // Start filtering only if there are enough values available and if filtering is enabled
        if (iteration > windowSize && filtering){

            // Filter data
            if (!filterNewDatapoint(&Amp1List, &smthdAmp1List, &aValue)) {
                qDebug() << f.errorString();
            }
            emit FilteringFinished(TimeList.rbegin()[0], smthdAmp1List.rbegin()[0]);

            // Wait till initial parameters for phase recognition are set by pressing 'Start Phase Recognition' button
            if(extrDetectionActive){

                // Check if there is a new max
                if (newMaxDetected(&smthdAmp1List, &maxIsLast, &lastMinInd, &newMaxInd)){

                    // Update indices of last 2 max
                    oldMaxInd = newMaxInd;
                    newMaxInd = TimeList.size()-3;

                    mutex.lock();

                    // Update time of newest (T) and last (T_old) breathing cycle
                    T_old = T;
                    T = abs(TimeList[newMaxInd] - TimeList[oldMaxInd]);

                    if (!checkBreathCycleDur(&T, &T_planning, &T_old, &phaseRecogActive)){
                        // Fallback for significant changes in duration of breathing cycle
                        phase = 0;
                        //phaseRecogActive = false; -> old code

                        //Only for validating phase recognition
                        IntervalPhaseList.fill(0);
                        PhaseList.append(IntervalPhaseList);
                        IntervalPhaseList.clear();
                    }
                    else {
                        // Update phase borders
                        dT = T /10.0;
                        //qDebug() << "TimeList[newMaxInd]" << TimeList[newMaxInd] << "filterShift" << filterShift;
                        correctedNewMaxTime = TimeList[newMaxInd] - filterShift;
                        //phaseRecogActive = true; -> old code
                        PhaseList.append(IntervalPhaseList);
                        IntervalPhaseList.clear();
                    }
                    mutex.unlock();
                }

                // Do phase recognition
                if (phaseRecogActive){
                    phaseRecognition(&TimeList.rbegin()[0], &correctedNewMaxTime, &dT, &phase, &extrDetectionActive, &phaseRecogActive);
                }

                emit showCurrentPhase(&phase);
            }
        }        

        // Set parameters for filtering
        else if (iteration == windowSize){
            fs = 1/(TimeList[1]-TimeList[0]);
            aValue = qSqrt( qPow( qCos(2*M_PI* fc/ fs), 2) - 4*qCos(2*M_PI* fc/ fs) + 3) + qCos(2*M_PI* fc/ fs) - 1;

            for (int u = 0; u < Amp1List.size(); u++){
                 smthdAmp1List.append(Amp1List[u]);
            }

            if (!filterNewDatapoint(&Amp1List, &smthdAmp1List, &aValue)) {
                qDebug() << f.errorString();
            }

            phaseRecogActive = true;

            double frst_t = TimeList.rbegin()[0];
            double frst_A = Amp1List.rbegin()[0];
            emit CreateChart(&frst_t, &frst_A, &smthdAmp1List.rbegin()[0]);
        }

        // Sleep for certain time to simulate dynamical reading
        this->msleep(this->sleepingTime);

        IntervalPhaseList.append(phase);
        }

    PhaseList.append(IntervalPhaseList);

    // Create csv file for testing phase recognition
    QString testOutputFileName = "SurfaceData/Measured/MEASURED_" + this->path + ".csv";
    fout.setFileName(testOutputFileName);
    if (!fout.open(QIODevice::WriteOnly|QIODevice::Text)) {
        qDebug() << fout.errorString();
    }
    QTextStream stream(&this->fout);

    if (TimeList.size() == PhaseList.size()){
        for(int i = 0; i < TimeList.size(); i++) {
            stream << TimeList[i] << ',' << PhaseList[i] << "\n";
        }
    }
    else{
        qDebug() << "Writing csv file was not successful";
        qDebug() << TimeList.size() << PhaseList.size();
    }

    this->fout.close();
    qDebug() << "Output file is closed";

    // Emit signal to start addAvgData() to adjust staticChart layout and to add the data
    emit DataReadingFinished(&TimeList, &Amp1List, &Amp2List, colNum);
}



