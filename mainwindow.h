#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringListModel>
#include <QDir>
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

    void computeFrequency();

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

    void on_squelchSlider_sliderReleased();

    void on_freqFineAdjustSlider_actionTriggered(int action);

    void on_statesListView_activated(const QModelIndex &index);

    void on_statesListView_clicked(const QModelIndex &index);

    void on_countiesListView_clicked(const QModelIndex &index);

    void on_updateChannelsButton_clicked();

    void on_sortByComboBox_currentIndexChanged(const QString &arg1);

    void on_categoryComboBox_currentIndexChanged(const QString &arg1);

    void on_sortByComboBox_currentTextChanged(const QString &arg1);

    void on_addToScanListBtn_clicked();

    void on_beginScanBtn_clicked();

    void on_stopScanButton_clicked();

    void on_findWifiBtn_clicked();

    void on_connectToWifiBtn_clicked();

private:
    bool widgetsReady = false;
    Ui::MainWindow *ui;
    Radio* radio;
    Waterfall* waterfall;
    QStringList keypadEntry;
    RadioStatus* radioStatus;
    AMQP* amqp;
    AMQPExchange * log_ex;
    AMQPQueue * log_qu;
    State* selected_state = nullptr;
    County* selected_county = nullptr;
    void initWidgets();

signals:
    void changeFrequency(double freq);
    void changeListenFreq(double freq);
    void changeBandwidth(double bw);
    void changeVolume(double vol);
    void changeSquelch(double squelch);
    void changeSearch(bool search);
    void changeScanStart(double freq);
    void changeScanStop(double freq);
    void changeScanStep(double freq);
    void setChannelScanList(QVector<Channel> channels);

};
#endif // MAINWINDOW_H
