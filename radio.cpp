#include "radio.h"

Radio::Radio(QObject *parent) :
    QThread(parent),
    amqp(new AMQP("localhost")),
    configMtx(new QMutex())
{

}

void Radio::setupRadio(){
    try{
        this->qu2 = amqp->createQueue("hello");
        this->qu2->Declare();
        this->qu2->Bind("amq.fanout", "hello");
        this->qu2->setConsumerTag("tag_123");
        amqp->printConnect();

    }catch(AMQPException e){
        qDebug() << e.getMessage().c_str() << Qt::endl;
    }
}


void Radio::setCenterFreq(double freq){
    this->centerFrequency = freq;
}


void Radio::setBandwidth(double bw){
    this->bandwidth = bw;
}

/**
 * @brief Radio::run called by QThread::start()
 */
void Radio::run(){
    forever{
        try{
            this->qu2->Get(AMQP_NOACK);

            AMQPMessage * m = this->qu2->getMessage();
            if(m != NULL){
//                qDebug() << "count: "<<  m->getMessageCount() << Qt::endl;
                if (m->getMessageCount() > -1) {
                    uint32_t j = 0;
                    QString msg = QString(m->getMessage(&j));
                    emit messageReady(msg); // messageReady signal
                    qDebug() << "message\n"<< msg << "\nmessage key: "<<  m->getRoutingKey().c_str() << Qt::endl;
                    qDebug() << "exchange: "<<  m->getExchange().c_str() << Qt::endl;
                    qDebug() << "Content-type: "<< m->getHeader("Content-type").c_str() << Qt::endl;
                    qDebug() << "Content-encoding: "<< m->getHeader("Content-encoding").c_str() << Qt::endl;
                }else{
//                    qDebug() << "No messages..." << Qt::endl;
                }
            }
        }catch(AMQPException e){
            qDebug() << e.getMessage().c_str() << Qt::endl;
        }
        QThread::usleep(1000); // 1 ms sleep
    }
}

/**
 * @brief Radio::configureRadio slot for configuring the radio
 * @param config
 */
void Radio::configureRadio(const RadioConfig& config){
    if(this->configMtx->tryLock()){

    }
}
