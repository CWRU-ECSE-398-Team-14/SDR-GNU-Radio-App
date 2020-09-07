#include "radio.h"

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
    this->update            = conf.update;
}

/**
 * @brief RadioConfig::packetizeData forms a packet from the config data
 * @param data the buffer to place the data into
 * @param max_len size of data buffer
 * @return number of bytes written to data
 */
QByteArray RadioConfig::packetizeData(){
    // add data to json object
    this->json->insert("centerFrequency", this->centerFrequency);
    this->json->insert("bandwidth", this->bandwidth);
    this->json->insert("fftPoints", this->fftPoints);
    this->json->insert("volume", this->volume);
    this->json->insert("scanStart", this->scanStartFreq);
    this->json->insert("scanStop", this->scanStopFreq);
    this->json->insert("scanStep", this->scanStep);

    return QJsonDocument(*(this->json)).toJson(QJsonDocument::Compact);
}

/**
 * @brief Radio::Radio object to represent the interface to the GNU radio process
 * @param parent
 */
Radio::Radio(QObject *parent) :
    QThread(parent),
    radioConfig(new RadioConfig()),
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
    qDebug() << QProcessEnvironment::systemEnvironment().keys();

    // create the GNU radio process
    this->radioProcess->start("python3", QStringList() << this->radioProgramPath << this->radioProgramArgs);
    this->radioProcess->waitForStarted(5000); // allow 5 seconds to start

    this->radioProcess->setReadChannel(QProcess::StandardOutput);
    qDebug() << this->radioProcess->readLine() << Qt::endl;

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
        qDebug() << e.getMessage().c_str() << Qt::endl;
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
 * @brief Radio::run called by QThread::start()
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
                    }
                }else{
//                    qDebug() << "No messages..." << Qt::endl;
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
                    QJsonObject finalPacket;

                    auto packet = this->radioConfig->packets.begin();
                    while(packet != this->radioConfig->packets.end()){
                        finalPacket.insert(packet->first, packet->second);
                        packet = this->radioConfig->packets.erase(packet);
                    }

                    QByteArray json = QJsonDocument(finalPacket).toJson(QJsonDocument::Compact);
                    this->ex->setHeader("Delivery-mode", 2);
                    this->ex->setHeader("Content-type", "application/json");
                    this->ex->setHeader("Content-encoding", "UTF-8");
                    this->ex->Publish(json.data(), json.size(), "");
                }
                this->configMtx->unlock();
            }
        }catch(AMQPException e){
            qDebug() << e.getMessage().c_str() << Qt::endl;
        }
        QThread::usleep(1000); // 1 ms sleep
    }
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

