#ifndef RADIO_H
#define RADIO_H

#include <QObject>
#include <QThread>
#include <QDebug>
#include <QMutex>
#include "amqpcpp/include/AMQPcpp.h"
#include "cstdio"

class RadioConfig : public QObject
{
    Q_OBJECT
public:
    explicit RadioConfig(QObject *parent = nullptr);
    double centerFrequency  = 500000.0; // 500 kHz
    double bandwidth        = 1000.0;   // 1 kHz
};


/**
 * @brief The Radio class, will run as a QThread
 */
class Radio : public QThread
{
    Q_OBJECT
    void run() override;
public:
    explicit Radio(QObject *parent = nullptr);
    void    setCenterFreq (double freq);
    void    setBandwidth  (double bw);
    double  getCenterFreq () { return centerFrequency; }
    double  getBandwidth  () { return bandwidth; }
    void    setupRadio    ();
public slots:
    void configureRadio   (const RadioConfig& config);

private:
    AMQP* amqp;
    AMQPQueue * qu2;
    QMutex* configMtx;
    double centerFrequency  = 500000.0; // 500 kHz
    double bandwidth        = 1000.0;   // 1 kHz

signals:
    void messageReady(const QString& msg);
};

#endif // RADIO_H
