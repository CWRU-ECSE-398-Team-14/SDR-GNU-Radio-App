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
#include <QDir>
#include <cstdio>
#include "AMQPcpp.h"
#include <limits>
#include "parse_csv.h"

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
    double listenFrequency  = 500000.0; //
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
    ~RadioStatus();
    RadioStatus(const RadioStatus& other); // copy constructor
    QString name        = "";
    QString statusStr   = "";
    double frequency    = 0.0; // what the radio is actually tuned-in to
    double signalPower  = -std::numeric_limits<double>::max(); // smallest representable number, -inf so to speak
    QString channelName = "";
    bool isSearching    = false;
};

class System{
public:
    System(QString name, int id);
    bool operator==(const System& ch);
    QString name    = "";
    int id          = 0;
    double frequency= 0.0;
    QString type    = "";
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
    friend bool operator==(const Channel& ch1, const Channel& ch2);
    QString name        = "";
    int     id          = 0; // denoted by the DEC column
    QString hex         = "";
    QString description = "";
    QString protocol    = "";
    QString mode        = "";
    QString type        = "";
    QString tag         = "";
    QString alpha_tag   = "";
    QString group       = "";
    QString talkgroup   = "";
    QString system      = "";
    double tone         = 0.0;
    double frequency    = 0.0;
    double bandwidth    = 0.0;
    int     systemId    = 0;
    QString getName() { return name; }
    QString toString();
};

/**
 *
 */
class County{
public:
    County(QString name, int id);
    QString name            = "None";
    int county_id           = 0;
    QString currentSystem   = "";
    QVector<QPair<QString, int>> systems;
    QVector<Channel> channels;
    void addSystem(QPair<QString, int> sys);
    QVector<QString> getProtocols();
    QVector<QString> getTags();
    QVector<QString> getTalkgroups();
    QVector<QString> getGroups();
    QVector<QPair<QString, int>> getSystems();
    QVector<Channel> getChannelsByProtocol(QString proto);
    QVector<Channel> getChannelsByTag(QString tag);
    QVector<Channel> getChannelsByTalkgroup(QString talkgroup);
    QVector<Channel> getChannelsByGroup(QString talkgroup);
    Channel* getChannelByAlphaTag(QString alphatag);
    Channel* getChannelByFrequency(double freq);
    Channel getChannelByString(QString str);
};

class State{
public:
    State(QString name);
    int numCounties() { return this->counties.length(); }
    QString name = "None";
    QVector<County> counties;
    QVector<System> systems;
    QStringList getCountyNames();
    County* getCountyByName(QString name);
};

static QStringList csv_headers = {"frequency", "description", "protocol", "tag", "alpha tag", "type"};

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
    QStringList protocols = {"P25", "FM"};
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
    QString countiesFilePath = "/home/adam/Documents/sdr_gnu_radio_app/tools/us_counties.csv";
    QString appDataDirPath = "/var/lib/sdrapp";
    QString scrapeProgramPath = "";
    QStringList radioProgramArgs = {""};
    QString statusStr;
    QVector<State> states;
    bool hasState(QString name);
    State* getStateByName(QString name);
    void addCountyToState(QString state_name, County county);
    QStringList getStateNames();
    int getCountyId(QString name);
    static QVector<Channel> channelsFromCsv(QVector<QVector<QString>> csv);
    static QVector<QVector<QString>> channelsToCsv(QVector<Channel> channels);
    void updateChannelsFromFile(QString state, QString county);
    void updateChannels(QString state, QString county, QVector<Channel> channels);
    static QVector<Channel> mergeChannels(QVector<Channel> oldChannels, QVector<Channel> newChannels);

public slots:
    void    setBandwidth    (double bw);
    void    setCenterFreq   (double freq);
    void    setListenFreq   (double freq);
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
    void    addChannelsToScanList(QVector<Channel> channels);
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

QJsonArray channelsToJson(QVector<Channel>& channels);

#endif // RADIO_H
