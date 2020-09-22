#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ==== rabbitMQ things ====
    this->amqp = new AMQP("localhost");

    // log messages exchange to publish log messages to
    this->log_ex = amqp->createExchange("logs");
    this->log_ex->Declare("logs", "fanout");
    this->log_qu = amqp->createQueue("event_logs");  // queue for radio config messages
    this->log_qu->Declare();
    this->log_qu->Bind("logs", ""); // bind to exchange named radio_fanout, no key

    this->logEvent("GUI setup started");

    // ==== radio thread things ====
    radio = new Radio(this);
    radioStatus = new RadioStatus(); // create this to avoid any null pointer issues

    // connect radio messageReady signal to MainWindow slot handleMessage
    connect(radio, &Radio::messageReady, this, &MainWindow::handleMessage);

    // connect radio statusUpdate signal to this object's handleStatusUpdate slot
    connect(radio, &Radio::statusUpdate, this, &MainWindow::handleStatusUpdate);

    // connect radio debug signal to main window's logMessage slot
    connect(radio, &Radio::debugMessage, this, &MainWindow::logMessage);

    connect(radio, &Radio::finished, radio, &QObject::deleteLater);

    radio->setupRadio(); // basic setup
    radio->start(); // start radio thread


    // ==== waterfall object ====
    // create a waterfall object
    waterfall = new Waterfall(this, ui->waterfallLabel->width(), ui->waterfallLabel->height());

    // connect waterfalls pixmapReady signal to main window's handleWaterfall slot
    connect(waterfall, &Waterfall::pixmapReady, this, &MainWindow::handleWaterfall);

    // connect radio's fftReady signal to waterfall's appendFFT slot
    connect(radio, &Radio::fftReady, waterfall, &Waterfall::appendFFT);

    // ==== MainWindow signals to Radio slots ====
    // connect mainwindow signals to radio slots
    connect(this, &MainWindow::changeFrequency, radio, &Radio::setCenterFreq);
    connect(this, &MainWindow::changeBandwidth, radio, &Radio::setBandwidth);
    connect(this, &MainWindow::changeVolume, radio, &Radio::setVolume);
    connect(this, &MainWindow::changeSquelch, radio, &Radio::setSquelch);
    connect(this, &MainWindow::changeSearch, radio, &Radio::setSearch);

    // ==== initialize widgets ====
    this->initWidgets();

    this->logEvent("GUI setup complete");
}

MainWindow::~MainWindow()
{
    delete ui;
    delete radio;
    delete waterfall;
}

/**
 * @brief MainWindow::initWidgets Initialize widgets to default values
 */
void MainWindow::initWidgets(){
    // ICONS

    // initialize widgets
    ui->centerFreqLcdNumber->display(QString("%1").arg(this->radio->getCenterFreq()/1.0e6, 0, 'f', 1));
    ui->bandwidthLcdNumber->display(QString("%1").arg(this->radio->getBandwidth()/1.0e3, 0, 'f', 1));
    ui->volumeSlider->setValue(ui->volumeSlider->maximum()/2);
    ui->squelchSlider->setValue(ui->squelchSlider->maximum()/2);

}

/**
 * @brief MainWindow::handleMessage slot to handle messages from radio object
 * @param msg
 */
void MainWindow::handleMessage(const QString& msg){
    this->logMessage(msg);
    qDebug() << msg << Qt::endl;
}

/**
 * @brief MainWindow::handleWaterfall slot to handle new waterfall data
 * @param pixmap the waterfall pixmap
 */
void MainWindow::handleWaterfall(const QPixmap& pixmap){
    ui->waterfallLabel->setPixmap(pixmap);
}

/**
 * @brief MainWindow::handleStatusUpdate slot for receiving radio status updates
 * @param status RadioStatus object representing the radio's current status
 */
void MainWindow::handleStatusUpdate(const RadioStatus& status){
    this->radioStatus = new RadioStatus(status);
    this->logMessage("SDR dongle status: " + radioStatus->statusStr);
}

void MainWindow::updateFreqDisplay(double freq){
    ui->activeFreqBtn->setText(QString("%1MHz").arg(freq/1.0e6, 0, 'f', 3));
    ui->centerFreqLcdNumber->display(QString("%1").arg(freq/1.0e6, 0, 'f', 1));
    Channel chan = this->radio->findChannelByFreq(freq);
    ui->currentChannelBtn->setText(chan.getName());
}

/**
 * @brief MainWindow::checkKeypadEntry check state of entered information
 */
void MainWindow::checkKeypadEntry(){

    QString entry = this->keypadEntry.join("");

    if(ui->activeFreqBtn->isChecked()){
        // modifying the active frequency
        ui->activeFreqBtn->setText(entry);
    }else if(ui->scanStartBtn->isChecked()){
        // modifying scan start frequency
        ui->scanStartBtn->setText(entry);
    }else if(ui->scanStopBtn->isChecked()){
        // modifying scan stop frequency
        ui->scanStopBtn->setText(entry);
    }else if(ui->scanStepBtn->isChecked()){
        // modifying scan step frequency
        ui->scanStepBtn->setText(entry);
    }

    // determine if it's time to update the frequency
    QStringList list = entry.split('.');
    if(list.size() > 1 && list[1].size() >= 3){
        bool ok = false;
        double d = entry.toDouble(&ok);
        if(ok){

            if(ui->activeFreqBtn->isChecked()){
                // modifying the active frequency
                d = d * 1.0e6; // from MHz to Hz
                d = constrain(d, this->radio->getMinFreq(), this->radio->getMaxFreq());

                this->updateFreqDisplay(d);

                int slider_value = int(map(d,
                                           this->radio->getMinFreq(),
                                           this->radio->getMaxFreq(),
                                           double(ui->frequencySlider->minimum()),
                                           double(ui->frequencySlider->maximum())) );
                ui->frequencySlider->setValue(slider_value);
                emit changeFrequency(d);
            }else if(ui->scanStartBtn->isChecked()){
                // modifying scan start frequency
                d = d * 1.0e6; // from MHz to Hz
                d = constrain(d, this->radio->getMinFreq(), this->radio->getMaxFreq());
                ui->scanStartBtn->setText(QString("%1MHz").arg(d/1.0e6, 0, 'f', 3));
            }else if(ui->scanStopBtn->isChecked()){
                // modifying scan stop frequency
                d = d * 1.0e6; // from MHz to Hz
                d = constrain(d, this->radio->getMinFreq(), this->radio->getMaxFreq());
                ui->scanStopBtn->setText(QString("%1MHz").arg(d/1.0e6, 0, 'f', 3));
            }else if(ui->scanStepBtn->isChecked()){
                // modifying scan step frequency
                d = d * 1.0e3; // from kHz to Hz
                d = constrain(d, 1000.0, 1000000.0);
                ui->scanStepBtn->setText(QString("%1kHz").arg(d/1.0e3, 0, 'f', 3));
                this->radio->setScanStep(d);
            }

        }else{
            // parsing failed, no changes to frequency of the radio
        }
        this->keypadEntry.clear(); // clear keypad entries so we can record a new input
    }
}

/**
 * @brief MainWindow::logMessage display message in the log viewer window
 * @param text message to print along with timestamp
 * this will also call logEvent to send the message to the logger service
 */
void MainWindow::logMessage(const QString& text){
    QDateTime dt = QDateTime::currentDateTime();

    // print to status pane
    ui->logViewer->appendPlainText(QString("[%1]: %2").arg(dt.toString()).arg(text));

    // send to logger service
    this->logEvent(text);
}

/**
 * @brief MainWindow::logEvent send a timestamped event message to the logger service
 * @param msg message to print as the event description
 */
void MainWindow::logEvent(const QString &msg){
    QDateTime dt = QDateTime::currentDateTime();

    // publish to rabbitMQ
    QJsonObject json;
    json.insert("timestamp", QJsonValue(double(dt.toMSecsSinceEpoch())/1000.0));
    json.insert("message", QJsonValue(msg));
    QJsonDocument doc(json);
    QByteArray arr = doc.toJson(QJsonDocument::Compact);
    this->log_ex->setHeader("Delivery-mode", 2);
    this->log_ex->setHeader("Content-type", "application/json");
    this->log_ex->setHeader("Content-encoding", "UTF-8");
    this->log_ex->Publish(arr.data(), arr.size(), "");
}

void MainWindow::on_frequencySlider_actionTriggered(int action)
{
    double slider_value = ui->frequencySlider->value();
    double freq = map(slider_value,
                      double(ui->frequencySlider->minimum()),
                      double(ui->frequencySlider->maximum()),
                      this->radio->getMinFreq(),
                      this->radio->getMaxFreq());
    emit changeFrequency(freq); // signal to radio the change frequency
    this->updateFreqDisplay(this->radio->getCenterFreq());
//    ui->activeFrequency->display(longs);
}

void MainWindow::on_bandwidthSlider_actionTriggered(int action)
{
    double slider_value = ui->bandwidthSlider->value();
    double bw = map(slider_value, double(ui->bandwidthSlider->minimum()), double(ui->bandwidthSlider->maximum()), 1.0e3, 3000.0e3);
    //this->radio->setBandwidth(bw);
    emit changeBandwidth(bw);
    const QString s = QString("%1").arg(this->radio->getBandwidth()/1.0e3, 0, 'f', 1);
    ui->bandwidthLcdNumber->display(s);
}

void MainWindow::on_frequencySlider_sliderPressed()
{

}

void MainWindow::on_volumeSlider_actionTriggered(int action)
{
    // adjust volume of radio audio
    emit changeVolume(map(ui->volumeSlider->value(), double(ui->volumeSlider->minimum()), double(ui->volumeSlider->maximum()), 0.0, 1.0));
}

// ==== BEGIN KEYPAD SLOTS ====
void MainWindow::on_pushButton_1_clicked()
{
    this->keypadEntry << "1";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_2_clicked()
{
    this->keypadEntry << "2";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_3_clicked()
{
    this->keypadEntry << "3";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_4_clicked()
{
    this->keypadEntry << "4";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_5_clicked()
{
    this->keypadEntry << "5";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_6_clicked()
{
    this->keypadEntry << "6";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_7_clicked()
{
    this->keypadEntry << "7";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_8_clicked()
{
    this->keypadEntry << "8";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_9_clicked()
{
    this->keypadEntry << "9";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_0_clicked()
{
    this->keypadEntry << "0";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_point_clicked()
{
    this->keypadEntry << ".";
    this->checkKeypadEntry();
}

void MainWindow::on_pushButton_pound_clicked()
{
    if(this->keypadEntry.size() > 0){
        this->keypadEntry.erase(--this->keypadEntry.end());
    }
    this->checkKeypadEntry();

}
// ==== END KEYPAD SLOTS ====


void MainWindow::on_scanStartBtn_toggled(bool checked)
{
    if(checked){
        this->keypadEntry.clear();
        ui->activeFreqBtn->setChecked(false);
        ui->scanStopBtn->setChecked(false);
        ui->scanStepBtn->setChecked(false);
    }else{
        // we just uncheck it, so process keypad entry
    }
}

void MainWindow::on_scanStopBtn_toggled(bool checked)
{
    if(checked){
        this->keypadEntry.clear();
        ui->scanStartBtn->setChecked(false);
        ui->activeFreqBtn->setChecked(false);
        ui->scanStepBtn->setChecked(false);
    }else{
        // we just uncheck it, so process keypad entry
    }
}

void MainWindow::on_activeFreqBtn_toggled(bool checked)
{
    if(checked){
        this->keypadEntry.clear();
        ui->scanStartBtn->setChecked(false);
        ui->scanStopBtn->setChecked(false);
        ui->scanStepBtn->setChecked(false);
    }else{
        // we just uncheck it, so process keypad entry
    }
}

void MainWindow::on_scanStepBtn_toggled(bool checked)
{
    if(checked){
        this->keypadEntry.clear();
        ui->scanStartBtn->setChecked(false);
        ui->scanStopBtn->setChecked(false);
        ui->activeFreqBtn->setChecked(false);
    }else{
        // we just uncheck it, so process keypad entry
    }
}

void MainWindow::on_decreaseBtn_clicked()
{
    // decrease active frequency by step size
    double f = this->radio->getCenterFreq();
    f -= this->radio->getScanStep();
    this->updateFreqDisplay(f);
    emit changeFrequency(f);

}

void MainWindow::on_increaseBtn_clicked()
{
    // increase active frequency by step size
    double f = this->radio->getCenterFreq();
    f += this->radio->getScanStep();
    this->updateFreqDisplay(f);
    emit changeFrequency(f);
}

void MainWindow::on_nextChannelBtn_clicked()
{

}

void MainWindow::on_prevChannelBtn_clicked()
{

}

void MainWindow::on_squelchSlider_actionTriggered(int action)
{
    emit changeSquelch(map(ui->squelchSlider->value(), double(ui->squelchSlider->minimum()), double(ui->squelchSlider->maximum()), 0.0, 1.0));
}

void MainWindow::on_searchBtn_clicked()
{
    // if not searching currently
    if(ui->searchBtn->text().compare("Search") == 0){
        // start searching
        emit changeSearch(true);
        ui->searchBtn->setText("Stop");
    }else{
        // stop searching
        emit changeSearch(false);
        ui->searchBtn->setText("Search");
    }

    // else

}
