#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // radio thread things
    radio = new Radio(this);
    radio->setupRadio(); // basic setup

    connect(radio, &Radio::messageReady, this, &MainWindow::handleMessage);
    connect(radio, &Radio::finished, radio, &QObject::deleteLater); // connect radio messageReady signal to MainWindow slot handleMessage
    radio->start();

    // initialize widgets
    ui->centerFreqLcdNumber->display(QString("%1").arg(this->radio->getCenterFreq()/1.0e6, 0, 'f', 1));
    ui->bandwidthLcdNumber->display(QString("%1").arg(this->radio->getBandwidth()/1.0e3, 0, 'f', 1));
}

MainWindow::~MainWindow()
{
    delete ui;
    delete radio;
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

void MainWindow::on_frequencySlider_actionTriggered(int action)
{
    double slider_value = ui->frequencySlider->value();
    double freq = map(slider_value, double(ui->frequencySlider->minimum()), double(ui->frequencySlider->maximum()), 0.5e6, 1000.0e6);
    this->radio->setCenterFreq(freq);
    const QString s = QString("%1").arg(this->radio->getCenterFreq()/1.0e6, 0, 'f', 1);
    ui->centerFreqLcdNumber->display(s);
}

void MainWindow::on_bandwidthSlider_actionTriggered(int action)
{
    double slider_value = ui->bandwidthSlider->value();
    double bw = map(slider_value, double(ui->bandwidthSlider->minimum()), double(ui->bandwidthSlider->maximum()), 1.0e3, 1000.0e3);
    this->radio->setBandwidth(bw);
    const QString s = QString("%1").arg(this->radio->getBandwidth()/1.0e3, 0, 'f', 1);
    ui->bandwidthLcdNumber->display(s);
}

void MainWindow::on_frequencySlider_sliderPressed()
{

}
