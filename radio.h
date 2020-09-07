#ifndef RADIO_H
#define RADIO_H

#include <QObject>
#include <QThread>
#include <QDebug>
#include <QMutex>
#include <QJsonObject>
#include <QJsonDocument>
#include <QProcess>
#include <cstdio>
#include "AMQPcpp.h"

/**
 * @brief The RadioConfig class
 * Config info for the GNU radio process
 */
class RadioConfig : public QObject
{
    Q_OBJECT
public:
    explicit RadioConfig(QObject *parent = nullptr);
    explicit RadioConfig(const RadioConfig& conf);
    QByteArray packetizeData();
    double minFreq          = 0.5e6;
    double maxFreq          = 1.7e9;
    double centerFrequency  = 500000.0; // 500 kHz
    double bandwidth        = 1000.0;   // 1 kHz
    double stepSize         = 100.0;    // step size for the FFT
    int fftPoints           = 450;      // number of points in the FFT
    double volume           = 0.0;      // value between 0 and 1
    double scanStartFreq    = 500000.0; // start scan frequency
    double scanStopFreq     = scanStartFreq + 1.0e6; // 1 MHz range
    double scanStep         = 12500.0;  // scan frequency step
    bool update             = false;    // flag to indicate that the SDR needs to be updated with config info
    QVector<QPair<QString,QJsonValue>> packets;       // store packets to send out
private:
    QJsonObject* json;
};


/**
 * @brief The Radio class, will run as a QThread
 * handles communication with the GNU radio process
 */
class Radio : public QThread
{
    Q_OBJECT
    void run() override;
public:
    explicit Radio(QObject *parent = nullptr);
    ~Radio();
    double  getCenterFreq () { return this->radioConfig->centerFrequency; }
    double  getBandwidth  () { return this->radioConfig->bandwidth; }
    double  getMaxFreq    () { return this->radioConfig->maxFreq; }
    double  getMinFreq    () { return this->radioConfig->minFreq; }
    double  getScanStep   () { return this->radioConfig->scanStep; }
    void    setupRadio    ();
    QString radioProgramPath = "/home/adam/Documents/hello_world/rcv.py";
    QStringList radioProgramArgs = {""};

public slots:
    void    setBandwidth    (double bw);
    void    setCenterFreq   (double freq);
    void    setVolume       (double volume);
    void    setFftPoints    (int points);
    void    setStartFreq    (double freq);
    void    setStopFreq     (double freq);
    void    setStepSize     (double freq);
    void    setScanStep     (double freq);
    void    configureRadio  (const RadioConfig& config);

private:
    RadioConfig * radioConfig;
    QProcess* radioProcess;
    AMQP* amqp;
    AMQPQueue * rxqu;
    AMQPExchange * ex;
    AMQPQueue * txqu;
    QMutex* configMtx;
    QVector<double> fft;
    void populateFFT(char* data, int size);
    double centerFrequency  = 500000.0; // 500 kHz
    double bandwidth        = 1000.0;   // 1 kHz

signals:
    void messageReady(const QString& msg);
    void fftReady(const QVector<double>& fft);
};

#endif // RADIO_H
