#ifndef RADIO_H
#define RADIO_H

#include <QObject>
#include <QThread>
#include <QDebug>
#include <QMutex>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QFile>
#include <QTimer>
#include <cstdio>
#include "AMQPcpp.h"
#include <limits>

/**
 * @brief The RadioConfig class
 * Config info for the GNU radio process
 */
class RadioConfig : public QObject
{
    Q_OBJECT
public:
    explicit RadioConfig(QObject *parent = nullptr);
    RadioConfig(const RadioConfig& conf);
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
    double squelch          = 0.0;
    bool   beginSearch      = false;
    bool update             = false;    // flag to indicate that the SDR needs to be updated with config info
    QVector<QPair<QString,QJsonValue>> packets;       // store packets to send out
    QString protocolStr     = "";
private:
    QJsonObject* json;
};

/**
 * @brief The RadioStatus class represents the radio status
 */
class RadioStatus : public QObject
{
    Q_OBJECT
public:
    explicit RadioStatus(QObject *parent = nullptr);
    RadioStatus(const RadioStatus& other); // copy constructor
    QString name        = "";
    QString statusStr   = "";
    double frequency    = 0.0; // what the radio is actually tuned-in to
    double signalPower  = -std::numeric_limits<double>::max(); // smallest representable number, -inf so to speak
    QString channelName = "";
    bool isSearching    = false;
};

/**
 * @brief The Channel class represents a radio channel
 */
class Channel
{
public:
    static Channel fromJson(QJsonObject json);
    static bool channelLessThan(const Channel& ch1, const Channel& ch2);
    explicit Channel(QString name = "", double freq = 0.0, double bw = 0.0, QString protocol = "");
    Channel(const Channel& ch);
    QJsonObject toJson();
    bool operator==(const Channel& ch);
    QString name = "";
    QString protocol = "";
    double frequency = 0.0;
    double bandwidth = 0.0;
    QString getName() { return name; }
};


/**
 * @brief The Radio class will run as a QThread
 * handles communication with the GNU radio process
 */
class Radio : public QThread
{
    Q_OBJECT
    void run() override;
public:
    explicit Radio(QObject *parent = nullptr);
    ~Radio();
    Channel findChannelByFreq(double freq);
    double  getCenterFreq () { return this->radioConfig->centerFrequency; }
    double  getBandwidth  () { return this->radioConfig->bandwidth; }
    double  getMaxFreq    () { return this->radioConfig->maxFreq; }
    double  getMinFreq    () { return this->radioConfig->minFreq; }
    double  getScanStep   () { return this->radioConfig->scanStep; }
    QString getProtocol   () { return this->radioConfig->protocolStr; }
    QString getStatusStr  () { return this->radioStatus->statusStr; }
    double  getFrequency  () { return this->radioStatus->frequency; } // what freq is the radio tuned to
    double  getSignalPower() { return this->radioStatus->signalPower; }
    QString getName       () { return this->radioStatus->name; }
    bool    isSearching   () { return this->radioStatus->isSearching; }
    void    setupRadio    ();
    QString radioProgramPath = "/home/adam/Documents/hello_world/rcv.py";
    QStringList radioProgramArgs = {""};
    QString statusStr;

public slots:
    void    setBandwidth    (double bw);
    void    setCenterFreq   (double freq);
    void    setVolume       (double volume);
    void    setFftPoints    (int points);
    void    setStartFreq    (double freq);
    void    setStopFreq     (double freq);
    void    setStepSize     (double freq);
    void    setScanStep     (double freq);
    void    setSquelch      (double squelch);
    void    setSearch       (bool search);
    void    configureRadio  (const RadioConfig& config);
    void    updateStatus    (const QJsonDocument& json);
    void    setProtocol     (const QString& str);
    void    addChannel      (const Channel& ch);
    void    saveChannels    ();
    void    nextChannel     ();
    void    prevChannel     ();
    void    readRadioProcessStdout (void);

private:
    RadioConfig * radioConfig;
    RadioStatus * radioStatus;
    QProcess* radioProcess;
    AMQP* amqp;
    AMQPQueue * rxqu;
    AMQPExchange * ex;
    AMQPQueue * txqu;
    QMutex* configMtx;
    QVector<double> fft;
    QVector<Channel> channels; // stores radio channels
    QString channelSavePath = "";
    QTimer * saveTimer;
    QVector<Channel>::iterator currentChannel;
    void populateFFT(char* data, int size);
    double centerFrequency  = 500000.0; // 500 kHz
    double bandwidth        = 1000.0;   // 1 kHz

signals:
    void messageReady(const QString& msg);
    void debugMessage(const QString& msg);
    void fftReady(const QVector<double>& fft);
    void statusUpdate(const RadioStatus& status);
};

#endif // RADIO_H
