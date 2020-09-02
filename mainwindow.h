#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "radio.h"
#include "waterfall.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void handleMessage(const QString &);
    void on_frequencySlider_sliderPressed();

    void on_frequencySlider_actionTriggered(int action);

    void on_bandwidthSlider_actionTriggered(int action);

private:
    Ui::MainWindow *ui;
    Radio* radio;
};
#endif // MAINWINDOW_H
