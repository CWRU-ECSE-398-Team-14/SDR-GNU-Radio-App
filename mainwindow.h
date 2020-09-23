#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "radio.h"
#include "waterfall.h"
#include "AMQPcpp.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void handleMessage(const QString &);

    void handleWaterfall(const QPixmap &);

    void handleStatusUpdate(const RadioStatus &);

    void logMessage(const QString &);

    void logEvent(const QString &);

private slots:
    void updateFreqDisplay(double freq);

    void checkKeypadEntry();

    void on_frequencySlider_sliderPressed();

    void on_frequencySlider_actionTriggered(int action);

    void on_bandwidthSlider_actionTriggered(int action);

    void on_volumeSlider_actionTriggered(int action);

    void on_pushButton_1_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_6_clicked();

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    void on_pushButton_9_clicked();

    void on_pushButton_0_clicked();

    void on_pushButton_point_clicked();

    void on_pushButton_pound_clicked();

    void on_scanStartBtn_toggled(bool checked);

    void on_scanStopBtn_toggled(bool checked);

    void on_activeFreqBtn_toggled(bool checked);

    void on_scanStepBtn_toggled(bool checked);

    void on_decreaseBtn_clicked();

    void on_increaseBtn_clicked();

    void on_nextChannelBtn_clicked();

    void on_prevChannelBtn_clicked();

    void on_squelchSlider_actionTriggered(int action);

    void on_searchBtn_clicked();

private:
    Ui::MainWindow *ui;
    Radio* radio;
    Waterfall* waterfall;
    QStringList keypadEntry;
    RadioStatus* radioStatus;
    AMQP* amqp;
    AMQPExchange * log_ex;
    AMQPQueue * log_qu;
    void initWidgets();

signals:
    void changeFrequency(double freq);
    void changeBandwidth(double bw);
    void changeVolume(double vol);
    void changeSquelch(double squelch);
    void changeSearch(bool search);
    void changeScanStart(double freq);
    void changeScanStop(double freq);
    void changeScanStep(double freq);

};
#endif // MAINWINDOW_H
