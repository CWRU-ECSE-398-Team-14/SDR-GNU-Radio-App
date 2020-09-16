#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ==== radio thread things ====
    radio = new Radio(this);
    radio->setupRadio(); // basic setup
    radioStatus = new RadioStatus(); // create this to avoid any null pointer issues

    // connect radio messageReady signal to MainWindow slot handleMessage
    connect(radio, &Radio::messageReady, this, &MainWindow::handleMessage);

    // connect radio statusUpdate signal to this object's handleStatusUpdate slot
    connect(radio, &Radio::statusUpdate, this, &MainWindow::handleStatusUpdate);
    connect(radio, &Radio::finished, radio, &QObject::deleteLater);
    radio->start(); // start radio thread


    // ==== waterfall object ====
    waterfall = new Waterfall(this, ui->waterfallLabel->width(), ui->waterfallLabel->height());

    // connect waterfalls pixmapReady signal to main window's handleWaterfall slot
    connect(waterfall, &Waterfall::pixmapReady, this, &MainWindow::handleWaterfall);

    // connect radio's fftReady signal to waterfall's appendFFT slot
    connect(radio, &Radio::fftReady, waterfall, &Waterfall::appendFFT);

    // initialize widgets
    ui->centerFreqLcdNumber->display(QString("%1").arg(this->radio->getCenterFreq()/1.0e6, 0, 'f', 1));
    ui->bandwidthLcdNumber->display(QString("%1").arg(this->radio->getBandwidth()/1.0e3, 0, 'f', 1));

    // connect widget signals to radio slots
    connect(this, &MainWindow::changeFrequency, radio, &Radio::setCenterFreq);
    connect(this, &MainWindow::changeBandwidth, radio, &Radio::setBandwidth);
    connect(this, &MainWindow::changeVolume, radio, &Radio::setVolume);

    // ICONS


}

MainWindow::~MainWindow()
{
    delete ui;
    delete radio;
    delete waterfall;
}

/**
 * @brief MainWindow::handleMessage slot to handle messages from radio object
 * @param msg
 */
void MainWindow::handleMessage(const QString& msg){
    ui->statusbar->clearMessage();
    ui->statusbar->showMessage(msg);
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
    ui->statusbar->showMessage("SDR dongle status: " + radioStatus->statusStr);
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
