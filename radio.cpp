#include "radio.h"


////////////////////////////////////////////////////////////////////////////////
//
//      RadioConfig
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief RadioConfig::RadioConfig default constructor
 * @param parent
 */
RadioConfig::RadioConfig(QObject *parent) :
    QObject(parent),
    json(new QJsonObject)
{

}

/**
 * @brief RadioConfig::RadioConfig copy constructor
 * @param conf the object to copy
 */
RadioConfig::RadioConfig(const RadioConfig& conf){
    this->json              = conf.json;
    this->minFreq           = conf.minFreq;
    this->maxFreq           = conf.maxFreq;
    this->centerFrequency   = conf.centerFrequency;
    this->bandwidth         = conf.bandwidth;
    this->stepSize          = conf.stepSize;
    this->fftPoints         = conf.fftPoints;
    this->volume            = conf.volume;
    this->scanStartFreq     = conf.scanStartFreq;
    this->scanStopFreq      = conf.scanStopFreq;
    this->scanStep          = conf.scanStep;
    this->squelch           = conf.squelch;
    this->beginSearch       = conf.beginSearch;
    this->update            = conf.update;
    this->protocolStr       = conf.protocolStr;
}

/**
 * @brief RadioConfig::packetizeData forms a packet from the config data
 * @param data the buffer to place the data into
 * @param max_len size of data buffer
 * @return number of bytes written to data
 */
QByteArray RadioConfig::packetizeData(){
    QJsonObject finalPacket;

    auto packet = this->packets.begin();
    while(packet != this->packets.end()){
        finalPacket.insert(packet->first, packet->second);
        packet = this->packets.erase(packet);
    }

    return QJsonDocument(finalPacket).toJson(QJsonDocument::Compact);
}


////////////////////////////////////////////////////////////////////////////////
//
//      RadioStatus
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief RadioStatus::RadioStatus default constructor
 * @param parent
 */
RadioStatus::RadioStatus(QObject *parent) :
    QObject(parent)
{

}

/**
 * @brief RadioStatus::RadioStatus copy constructor
 * @param status
 */
RadioStatus::RadioStatus(const RadioStatus& status){
    this->statusStr = status.statusStr;
    this->frequency = status.frequency;
    this->signalPower = status.signalPower;
    this->name      = status.name;
    this->channelName = status.channelName;
    this->isSearching = status.isSearching;
}


////////////////////////////////////////////////////////////////////////////////
//
//      Channel
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Channel::fromJson create Channel object from a QJSonObject
 * @param json the QJsonObject presumably containing our desired data
 * @return a new Channel object
 */
Channel Channel::fromJson(QJsonObject json){
    Channel retval;
    retval.name = json.value("name").toString();
    retval.protocol = json.value("protocol").toString();
    retval.frequency = json.value("frequency").toDouble();
    retval.bandwidth = json.value("bandwidth").toDouble();
    return retval;
}

bool Channel::channelLessThan(const Channel& ch1, const Channel& ch2){
    return ch1.frequency < ch2.frequency;
}

/**
 * @brief Channel::Channel default constructor
 * @param name name of this channel
 * @param freq frequency of this channel in Hz
 * @param bw bandwidth of this channel in Hz
 * @param protocol the protocol this channel is expected to use
 */
Channel::Channel(QString name, double freq, double bw, QString protocol) {
    this->name = name;
    this->frequency = freq;
    this->bandwidth = bw;
    this->protocol = protocol;
}

/**
 * @brief Channel::Channel copy constructor
 * @param ch
 */
Channel::Channel(const Channel& ch){
    name = ch.name;
    protocol = ch.protocol;
    frequency = ch.frequency;
    bandwidth = ch.bandwidth;
}

bool Channel::operator==(const Channel& ch){
    bool retval = this->frequency == ch.frequency;
    if(retval){
        retval &= this->bandwidth == ch.bandwidth;
        if(retval){
            retval &= this->name.compare(ch.name) == 0;
            if(retval){
                retval &= this->protocol.compare(ch.protocol) == 0;
            }
        }

    }

    return retval;
}

QJsonObject Channel::toJson(){
    QJsonObject json;
    json.insert("name", this->name);
    json.insert("protocol", this->protocol);
    json.insert("frequency", this->frequency);
    json.insert("bandwidth", this->bandwidth);
    return json;
}


////////////////////////////////////////////////////////////////////////////////
//
//      Radio
//
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Radio::Radio object to represent the interface to the GNU radio process
 * @param parent
 */
Radio::Radio(QObject *parent) :
    QThread(parent),
    radioConfig(new RadioConfig()),
    radioStatus(new RadioStatus()),
    radioProcess(new QProcess(parent)),
    amqp(new AMQP("localhost")),
    configMtx(new QMutex())
{

}

Radio::~Radio(){
    this->radioProcess->kill();
}

/**
 * @brief Radio::setupRadio forms AMQP objects and other basic setup
 */
void Radio::setupRadio(){
    // get environment variables
    QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();

    if(sys.contains("HOME")){
        this->channelSavePath = sys.value("HOME") + "/.channels.json";
    }

    if(sys.contains("GNU_RADIO_PROCESS_PATH")){
        QString fname = sys.value("GNU_RADIO_PROCESS_PATH");
        if(QFile::exists(fname)){
            this->radioProgramPath = fname;
            emit debugMessage(QString("GNU_RADIO_PROCESS_PATH = %1").arg(this->radioProgramPath));
        }
    }

    // create the GNU radio process
    this->radioProcess->start("python3", QStringList() << this->radioProgramPath << this->radioProgramArgs);
    this->radioProcess->waitForStarted(5000); // allow 5 seconds to start

    this->radioProcess->setReadChannel(QProcess::StandardOutput);
    emit debugMessage(QString("Started GNU radio process: %1").arg(QString(this->radioProcess->readLine())));

    // create the AMQP objects
    try{
        this->rxqu = amqp->createQueue("radio_data");
        this->rxqu->Declare();
        this->rxqu->Bind("amq.fanout", "hello"); // bind to amq.fanout, key=hello
        this->rxqu->setConsumerTag("tag_sdr_gui");

        // declare a new exchange
        this->ex = amqp->createExchange("radio_fanout"); // new exchange named radio_fanout
        this->ex->Declare("radio_fanout", "fanout");     // name: radio_fanout, type: fanout

        // transmit radio settings queue
        this->txqu = amqp->createQueue("radio_config");  // queue for radio config messages
        this->txqu->Declare();
        this->txqu->Bind("radio_fanout", ""); // bind to exchange named radio_fanout, no key

        amqp->printConnect();

    }catch(AMQPException e){
        emit debugMessage(QString(e.getMessage().c_str()));
        qDebug() << e.getMessage().c_str() << Qt::endl;
    }

    // load channels from file
    if(QFile::exists(this->channelSavePath)){
        emit debugMessage("Loading channels from file...");
        qDebug() << "Loading channels from file..." << Qt::endl;
        QFile file(this->channelSavePath);
        file.open(QIODevice::ReadOnly);
        QJsonArray values = QJsonDocument::fromJson(file.readAll()).array();
        file.close();

        if(values.size() > 0){
            for(auto value : values){
                Channel::fromJson( value.toObject() );
            }
        }
        emit debugMessage(QString("Loaded %1 channels from file").arg(this->channels.size()));
        qDebug() << "Loaded " << this->channels.size() << " channels from file" << Qt::endl;
     }

    // create a timer to save our radio channels periodically
    this->saveTimer = new QTimer(this);
//    this->saveTimer->setInterval(10000); // save every 10 seconds
    connect(this->saveTimer, &QTimer::timeout, this, &Radio::saveChannels);
    this->saveTimer->start(10000);
}

/**
 * @brief Radio::findChannelByFreq finds and returns a copy of the channel at given frequency
 * @param freq frequency of the channel
 * @return copy of the corresponding channel or an empty channel object if no match found
 */
Channel Radio::findChannelByFreq(double freq){
    int i = 0;
    while(i < channels.size() && channels.at(i).frequency != freq){
        i++;
    }
    if(i < channels.size()){
        return channels.at(i);
    }else{
        return Channel();
    }
}

/**
 * @brief Radio::setCenterFreq update radioConfig centerFrequency and set update flag
 * @param freq
 */
void Radio::setCenterFreq(double freq){
    this->configMtx->lock();
    this->radioConfig->centerFrequency = freq;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("centerFrequency", QJsonValue(freq)));
    this->configMtx->unlock();
}

/**
 * @brief Radio::setFftPoints update radioConfig fftPoints and set update flag
 * @param points desired number of fft points
 */
void Radio::setFftPoints(int points){
    this->configMtx->lock();
    this->radioConfig->fftPoints = points;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("fftPoints", QJsonValue(points)));
    this->configMtx->unlock();
}

/**
 * @brief Radio::setBandwidth update radioConfig bandwidth and set update flag
 * @param bw
 */
void Radio::setBandwidth(double bw){
    this->configMtx->lock();
    this->radioConfig->bandwidth = bw;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("bandwidth", QJsonValue(bw)));
    this->configMtx->unlock();
}

/**
 * @brief Radio::setVolume slot to set radioConfig volume and set update flag
 * @param volume value between 0.0 and 1.0. volume will be clamped to this range
 */
void Radio::setVolume(double volume){
    if(volume > 1.0){
        volume = 1.0;
    }else if(volume < 0.0){
        volume = 0.0;
    }
    this->configMtx->lock();
    this->radioConfig->volume = volume;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("volume", QJsonValue(volume)));
    this->configMtx->unlock();
}

void Radio::setStartFreq(double freq){
    this->configMtx->lock();
    this->radioConfig->scanStartFreq = freq;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("scanStart", QJsonValue(freq)));
    this->configMtx->unlock();
}
void Radio::setStopFreq(double freq){
    this->configMtx->lock();
    this->radioConfig->scanStopFreq = freq;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("scanStop", QJsonValue(freq)));
    this->configMtx->unlock();
}
void Radio::setStepSize(double freq){
    this->configMtx->lock();
    this->radioConfig->stepSize = freq;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("fftStep", QJsonValue(freq)));
    this->configMtx->unlock();
}
void Radio::setScanStep(double freq){
    this->configMtx->lock();
    this->radioConfig->scanStep = freq;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("scanStep", QJsonValue(freq)));
    this->configMtx->unlock();
}
void Radio::setSquelch(double squelch){
    if(squelch > 1.0){
        squelch = 1.0;
    }else if(squelch < 0.0){
        squelch = 0.0;
    }
    this->configMtx->lock();
    this->radioConfig->squelch = squelch;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("squelch", QJsonValue(squelch)));
    this->configMtx->unlock();
}
void Radio::setSearch(bool search){
    this->configMtx->lock();
    this->radioConfig->beginSearch = search;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("beginSearch", QJsonValue(search)));
    this->configMtx->unlock();
}
/**
 * @brief Radio::configureRadio slot for updating radioConfig
 * @param config
 */
void Radio::configureRadio(const RadioConfig& config){
    this->configMtx->lock();
    this->radioConfig = new RadioConfig(config);
    this->radioConfig->update = true;
    this->configMtx->unlock();
}

/**
 * @brief Radio::updateStatus update radioStatus based on a json document
 * @param json json document with status updates
 */
void Radio::updateStatus(const QJsonDocument& jsonDoc){
    QJsonObject json = jsonDoc.object();
    QStringList keys = json.keys();
    for(int i = 0; i < keys.size(); i++){
        QString s = keys[i];
        if(s.compare("status") == 0){
            this->radioStatus->statusStr = json.value(s).toString();
        }else if(s.compare("frequency") == 0){
            this->radioStatus->frequency = json.value(s).toDouble();
        }else if(s.compare("signalPower") == 0){
            this->radioStatus->signalPower = json.value(s).toDouble();
        }else if(s.compare("name") == 0){
            this->radioStatus->name = json.value(s).toString();
        }else if(s.compare("channelName") == 0){
            this->radioStatus->channelName = json.value(s).toString();
        }else if(s.compare("isSearching") == 0){
            this->radioStatus->isSearching = json.value(s).toBool();
        }
    }
    // this might be a bad thing to do maybe? cuz radioStatus is private?
    emit statusUpdate(*this->radioStatus);
}

void Radio::setProtocol(const QString& str){
    this->configMtx->lock();
    this->radioConfig->protocolStr = str;
    this->radioConfig->packets.append(QPair<QString, QJsonValue>("protocol", QJsonValue(this->radioConfig->protocolStr)));
    this->configMtx->unlock();
}

/**
 * @brief Radio::addChannel slot to add a new channel and sort the vector
 * @param ch channel object to add to channels vector
 */
void Radio::addChannel(const Channel& ch){
    // first, see if it exists already
    int i = 0;
    while(i < channels.size() && channels.at(i).frequency != ch.frequency){
        i++;
    }
    if(i < channels.size()){
        // channel already exists, update it
        channels.replace(i, ch);
    }else{
        // new channel
        double f = currentChannel->frequency;

        // add then sort the vector
        this->channels.push_back(ch);
        std::sort(channels.begin(), channels.end(), Channel::channelLessThan);

        i = 0;
        while(i < channels.size() && channels.at(i).frequency != f){
            i++;
        }
        if(i < channels.size()){
            currentChannel = channels.begin() + i;
        }else{
            currentChannel = channels.begin();
        }


    }

}

/**
 * @brief Radio::saveChannels slot for saving channels to disk in JSON format
 */
void Radio::saveChannels(){
    if(channels.size() > 0){
        emit debugMessage("Saving channels...");
        qDebug() << "Saving channels..." << Qt::endl;
        QJsonArray arr;
        for(auto channel : this->channels){
            arr.append(QJsonValue(channel.toJson()));
        }
        QFile f(this->channelSavePath);
        f.open(QIODevice::ReadWrite | QIODevice::Text);
        QTextStream out(&f);
        QJsonDocument json(arr);
        out << json.toJson();
        f.close();
    }
}

void Radio::nextChannel(){
    if(channels.size() > 0){
        currentChannel++;
        if(currentChannel == channels.end()){
            currentChannel = channels.begin();
        }
    }

}

void Radio::prevChannel(){
    if(channels.size() > 0){
        if(currentChannel == channels.begin()){
            currentChannel = channels.end();
        }else{
            currentChannel--;
        }
    }
}

/**
 * @brief Radio::populateFFT populate this object's fft vector from char array containing doubles
 * @param data raw data
 * @param size number of bytes in data
 */
void Radio::populateFFT(char* data, int size){
    int doubles = size/sizeof(double);
    this->fft.resize(doubles);
    double value;
    for(int i = 0; i < size; i+=sizeof(double)){
        memcpy(&value, data + i, sizeof(double));
        this->fft[i/sizeof(double)] = value;
    }
}

/**
 * @brief Radio::run called by QThread::start(), main loop
 * Checks for incoming messages on rxqu and checks if the config has been updated
 * if radioConfig has been updated, it sends out config data on txqu
 */
void Radio::run(){
    forever{
        try{
            // check for data messages from the GNU radio process
            this->rxqu->Get(AMQP_NOACK);

            AMQPMessage * m = this->rxqu->getMessage();
            if(m != NULL){
                if (m->getMessageCount() > -1) {
                    uint32_t j = 0;
                    QString contentType = QString(m->getHeader("Content-type").c_str());
                    if(contentType.compare("text/plain") == 0){
                        QString msg = QString(m->getMessage(&j));
                        qDebug() << "message\n"<< msg << "\nmessage key: "<<  m->getRoutingKey().c_str() << Qt::endl;
                        qDebug() << "exchange: "<<  m->getExchange().c_str() << Qt::endl;
                        qDebug() << "Content-type: "<< contentType << Qt::endl;
                        qDebug() << "Content-encoding: "<< m->getHeader("Content-encoding").c_str() << Qt::endl;

                        emit messageReady(msg); // messageReady signal
                    }else if(contentType.compare("application/octet-stream") == 0){
                        char* raw_data = m->getMessage(&j);
                        this->populateFFT(raw_data, j);
                        emit fftReady(this->fft); // fftReady signal
                    }else if(contentType.compare("application/json") == 0){
                        // some radio status info incoming
                        this->updateStatus(QJsonDocument::fromJson(QByteArray(m->getMessage(&j))));
                    }
                }
            }

            // try to lock the config mutex
            if(this->configMtx->try_lock()){
                // check if the update flag is set (old and probably can be removed)
                if(this->radioConfig->update){
                    QByteArray json = this->radioConfig->packetizeData();
                    this->ex->setHeader("Delivery-mode", 2);
                    this->ex->setHeader("Content-type", "application/json");
                    this->ex->setHeader("Content-encoding", "UTF-8");
                    this->ex->Publish(json.data(), json.size(), "");
                    this->radioConfig->update = false; // reset flag
                }
                // check for key value pairs
                if(this->radioConfig->packets.size() > 0){
                    QByteArray json = this->radioConfig->packetizeData();
                    this->ex->setHeader("Delivery-mode", 2);
                    this->ex->setHeader("Content-type", "application/json");
                    this->ex->setHeader("Content-encoding", "UTF-8");
                    this->ex->Publish(json.data(), json.size(), "");
                }
                this->configMtx->unlock();
            }
        }catch(AMQPException e){
            emit debugMessage(QString(e.getMessage().c_str()));
            qDebug() << e.getMessage().c_str() << Qt::endl;
        }
        QThread::usleep(1000); // 1 ms sleep
    }
}
