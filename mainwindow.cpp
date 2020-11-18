#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    this->startMSecs = QDateTime::currentMSecsSinceEpoch();

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
    qRegisterMetaType<RadioStatus>("RadioStatus");
    connect(radio, &Radio::statusUpdate, this, &MainWindow::handleStatusUpdate);

    // connect radio debug signal to main window's logMessage slot
    connect(radio, &Radio::debugMessage, this, &MainWindow::logMessage);

    connect(radio, &Radio::finished, radio, &QObject::deleteLater);

    // ==== waterfall object ====
    // create a waterfall object
    waterfall = new Waterfall(this, ui->waterfallLabel->width(), ui->waterfallLabel->height());

    // connect waterfalls pixmapReady signal to main window's handleWaterfall slot
    connect(waterfall, &Waterfall::pixmapReady, this, &MainWindow::handleWaterfall);

    // connect radio's fftReady signal to waterfall's appendFFT slot
    connect(radio, &Radio::fftReady, waterfall, &Waterfall::appendFFT);

    radio->setupRadio(); // basic setup
    radio->start(); // start radio thread

    // ==== MainWindow internal signal slot connections ====
    this->scrapeSystemsProc = new QProcess(this);
    connect(scrapeSystemsProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::scrapeSystemsHandleStdout); //&QProcess::readyReadStandardOutput

    // web scraping process finished signal to corresponding slot
    this->webScrapeProc = new QProcess(this);
    connect(webScrapeProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::scrapeChannelsHandleStdout);

    // web scraping timeout timer
    this->webScrapeTimeoutTimer = new QTimer(this);
    connect(this->webScrapeTimeoutTimer, &QTimer::timeout, this, &MainWindow::webScrapeTimeout);


    // ==== MainWindow signals to Radio slots ====
    // connect mainwindow signals to radio slots
    connect(this, &MainWindow::changeFrequency, radio, &Radio::setCenterFreq);
    connect(this, &MainWindow::changeListenFreq, radio, &Radio::setListenFreq);
    connect(this, &MainWindow::changeBandwidth, radio, &Radio::setBandwidth);
    connect(this, &MainWindow::changeVolume, radio, &Radio::setVolume);
    connect(this, &MainWindow::changeSquelch, radio, &Radio::setSquelch);
    connect(this, &MainWindow::changeSearch, radio, &Radio::setSearch);
    connect(this, &MainWindow::changeScanStart, radio, &Radio::setStartFreq);
    connect(this, &MainWindow::changeScanStop, radio, &Radio::setStopFreq);
    connect(this, &MainWindow::changeScanStep, radio, &Radio::setScanStep);
    connect(this, &MainWindow::setChannelScanList, radio, &Radio::addChannelsToScanList);
    connect(this, &MainWindow::changeProtocol, radio, &Radio::setProtocol);

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

    // ==== SETUP TAB ====
    // populate states scroll area with state names
    QAbstractItemModel *model = new QStringListModel(this->radio->getStateNames());
    ui->setupListView->setModel(model);
    this->setupState = MainWindow::SELECT_STATE;

    ui->updateChannelsButton->setDisabled(true);

    ui->setupStateBackButton->setDisabled(true);
    ui->setupStateNextButton->setDisabled(false);

    // ==== SCAN LIST TAB ====
    // scan list ListView initial setup
    ui->scanListListView->setModel(new QStringListModel());

    // set p25 button to checked
    ui->p25Btn->click(); // check button and emit changeProtocol signal

    // ==== waterfall display setup ====
    //      freq fine adjust slider
    ui->freqFineAdjustSlider->setValue((ui->freqFineAdjustSlider->maximum() + ui->freqFineAdjustSlider->minimum())/2);
    this->setBandwidthSetpoint(12500.0);

    //      limit labels on waterfall display
    double center   = this->getCenterFreqSetpoint(); // wherever it happens to be
    double bw       = this->getBandwidthSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((center - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((center + bw/2)/1.0e6, 0, 'f', 4));

}

/**
 * @brief MainWindow::getBandwidthSetpoint find bandwidth setpoint according to the bandwidth slider
 * @return bandwidth in Hz according to bandwidthSlider position
 */
double MainWindow::getBandwidthSetpoint(){
    double slider_value = ui->bandwidthSlider->value();
    return map(slider_value, double(ui->bandwidthSlider->minimum()), double(ui->bandwidthSlider->maximum()), 1.0e3, 3000.0e3);
}

double MainWindow::getCenterFreqSetpoint(){
    double slider_value = ui->frequencySlider->value();
    return map(slider_value,
                      double(ui->frequencySlider->minimum()),
                      double(ui->frequencySlider->maximum()),
                      this->radio->getMinFreq(),
                      this->radio->getMaxFreq());
}

/**
 * @brief MainWindow::getFreqFineAdjustOffset return center freq. offset due to the fine adjust slider position
 * @return frequency offset in Hz
 */
double MainWindow::getFreqFineAdjustOffset(){
    double slider_value = ui->freqFineAdjustSlider->value();
    double bw = this->getBandwidthSetpoint();
    double offset = map(slider_value, ui->freqFineAdjustSlider->minimum(), ui->freqFineAdjustSlider->maximum(), -bw/2.0, bw/2.0);
    return offset;
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
//    this->radioStatus = new RadioStatus(status);
    this->logMessage("SDR dongle status: " + status.statusStr); // debug output

    // ==== Scan/Search tab ====
    // set button texts in the first tab
    ui->currentChannelBtn->setText(status.channelName);
    this->updateFreqDisplay(status.frequency); // updates the active frequency display button

    // ==== scan list tab ====
    // construct text block
    QString channelInfo = QString("%1\r\n%2MHz\r\n%3dBm")
            .arg(status.channelName)
            .arg(status.frequency/1e6, 0, 'g', 4)
            .arg(status.signalPower, 0, 'g', 2);

    ui->currentChannelInfoLbl->setText(channelInfo); // display in Scan List tab

    // ==== waterfall tab ====
    this->setCenterFreqSetpoint(status.frequency);
//    ui->centerFreqLcdNumber->display(QString("%1").arg(status.frequency/1e6, 0, 'f', 1));


}

/**
 * @brief MainWindow::updateFreqDisplay display formatted freq on the main tab
 * @param freq frequency to display in Hz
 */
void MainWindow::updateFreqDisplay(double freq){
    ui->activeFreqBtn->setText(QString("%1MHz").arg(freq/1.0e6, 0, 'f', 3));
}

void MainWindow::computeFrequency(){
    //
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

                setCenterFreqSetpoint(d);
                emit changeListenFreq(d);
            }else if(ui->scanStartBtn->isChecked()){
                // modifying scan start frequency
                d = d * 1.0e6; // from MHz to Hz
                d = constrain(d, this->radio->getMinFreq(), this->radio->getMaxFreq());
                ui->scanStartBtn->setText(QString("%1MHz").arg(d/1.0e6, 0, 'f', 3));
                emit changeScanStart(d);
            }else if(ui->scanStopBtn->isChecked()){
                // modifying scan stop frequency
                d = d * 1.0e6; // from MHz to Hz
                d = constrain(d, this->radio->getMinFreq(), this->radio->getMaxFreq());
                ui->scanStopBtn->setText(QString("%1MHz").arg(d/1.0e6, 0, 'f', 3));
                emit changeScanStop(d);
            }else if(ui->scanStepBtn->isChecked()){
                // modifying scan step frequency
                d = d * 1.0e3; // from kHz to Hz
                d = constrain(d, 1000.0, 1000000.0);
                ui->scanStepBtn->setText(QString("%1kHz").arg(d/1.0e3, 0, 'f', 3));
                emit changeScanStep(d);
//                this->radio->setScanStep(d);
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
    double deltaT = (QDateTime::currentMSecsSinceEpoch() - this->startMSecs)/1000.0;

    // print to status pane
    ui->logViewer->appendPlainText(QString("[%1]: %2").arg(deltaT, 10, 'f', 3).arg(text));

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

/**
 * @brief MainWindow::setCenterFreqSetpoint programmatically set the center freq. slider to given freq.
 * @param freq frequency to set the slider to
 */
void MainWindow::setCenterFreqSetpoint(double freq){
    int slider_value = int(map(freq,
                               this->radio->getMinFreq(),
                               this->radio->getMaxFreq(),
                               double(ui->frequencySlider->minimum()),
                               double(ui->frequencySlider->maximum())) );
    ui->frequencySlider->setValue(slider_value);

    double bw = this->getBandwidthSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((freq - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((freq + bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelCenter->setText(QString("%1MHz").arg(freq/1.0e6, 0, 'f', 4));
}

/**
 * @brief MainWindow::setBandwidthSetpoint programmatically set bandwidth slider to given bandwidth
 * @param bw bandwidth to set the slider to
 */
void MainWindow::setBandwidthSetpoint(double bw){
    int slider_value = int(map(bw, 1.0e3, 3000.0e3,
                               double(ui->bandwidthSlider->minimum()),
                               double(ui->bandwidthSlider->maximum())) );
    ui->bandwidthSlider->setValue(slider_value);

    ui->bandwidthLcdNumber->display(QString("%1").arg(bw/1.0e3, 0, 'f', 1));

    double freq = this->getCenterFreqSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((freq - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((freq + bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelCenter->setText(QString("%1MHz").arg(freq/1.0e6, 0, 'f', 4));
}

void MainWindow::on_frequencySlider_actionTriggered(int action)
{
    double center = this->getCenterFreqSetpoint(); // pull value from slider position
    double freq = center + this->getFreqFineAdjustOffset(); // consider the offset from fine adjust slider
    emit changeFrequency(freq); // signal to radio the change frequency
    // this->updateFreqDisplay(this->radio->getCenterFreq());
    ui->centerFreqLcdNumber->display(QString("%1").arg(freq/1.0e6, 0, 'f', 1));
//    ui->activeFrequency->display(longs);

    double bw = this->getBandwidthSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((center - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((center + bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelCenter->setText(QString("%1MHz").arg(center/1.0e6, 0, 'f', 4));

}

void MainWindow::on_bandwidthSlider_actionTriggered(int action)
{
    double bw = getBandwidthSetpoint();
    //this->radio->setBandwidth(bw);
    emit changeBandwidth(bw);
    const QString s = QString("%1").arg(this->radio->getBandwidth()/1.0e3, 0, 'f', 1);
    ui->bandwidthLcdNumber->display(s);

    double center = this->getCenterFreqSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((center - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((center + bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelCenter->setText(QString("%1MHz").arg(center/1.0e6, 0, 'f', 4));
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
    // emit changeSquelch(map(ui->squelchSlider->value(), double(ui->squelchSlider->minimum()), double(ui->squelchSlider->maximum()), 0.0, 1.0));
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

void MainWindow::on_squelchSlider_sliderReleased()
{
    emit changeSquelch(map(ui->squelchSlider->value(), double(ui->squelchSlider->minimum()), double(ui->squelchSlider->maximum()), 0.0, 1.0));
}

void MainWindow::on_freqFineAdjustSlider_actionTriggered(int action)
{
    // change LCD display
    double f = this->radio->getCenterFreq() + this->getFreqFineAdjustOffset();
    ui->centerFreqLcdNumber->display(QString("%1").arg(f/1.0e6, 0, 'f', 1));

    // TODO: adjust the frequency we're tuned to
}

/**
 * @brief MainWindow::beginWebScraping slot to construct the call to the web scraping script
 */
void MainWindow::beginWebScraping(){
    if(selected_state != nullptr && selected_county != nullptr){
        this->logMessage(QString("Updating channels for %2 County, %1").arg(selected_state->name).arg(selected_county->name));
    }else{
        this->logMessage("Not updating channels:");
        if(selected_state == nullptr)
            this->logMessage(" - selected_state is a null pointer");
        if(selected_county == nullptr)
            this->logMessage(" - selected_county is a null pointer");
        return;
    }
    // see what protocol the radio is using right now
//    if(this->radio->getProtocol().compare("p25", Qt::CaseInsensitive) == 0){
//        // need to figure out selected system and pull channels based on that
//        QString sysStr = ui->setupListView->currentIndex().data().toString();
//        QStringList sysList = sysStr.split(':');
//        this->currentSystem = QPair<QString, int>(sysList[1], sysList[0].toInt());
//    }

    // call the web scraping program and process it's output
    // 1. construct URL
    // 2. call web scraping program and pass it the URL
    QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();
    QString path = "";

    // check for env variable existance
    if(sys.contains("WEB_SCRAPE_PROGRAM_PATH")){
        path = sys.value("WEB_SCRAPE_PROGRAM_PATH");
    }else{
        logMessage("web scraping script path unknown.");
        return;
    }

    // 3. start the program
    if(QFile::exists(path)){
        this->logMessage("starting web scraping...");
        QDir tmpDir(QString("/var/lib/sdrapp/%1").arg("tmp"));
        if(!tmpDir.exists()){
            logMessage("Creating tmp directory...");
            tmpDir.mkpath(tmpDir.absolutePath());
        }
        webScrapeProc->setStandardOutputFile(QString("%1/webscrape_results.csv").arg(tmpDir.absolutePath()));

        // begin the process, pass in the current System ID
        logMessage(QString("%1: %2").arg("Current system ID").arg(this->currentSystem.second));
        webScrapeProc->start("python3", QStringList() << path << QString("%1").arg(this->currentSystem.second));
    }else{
        this->logMessage("web scraping script does not exist.");
        return;
    }
    this->webScrapeTimeoutTimer->start(10000); // 10 seconds
}

/**
 * @brief MainWindow::on_updateChannelsButton_clicked
 */
void MainWindow::on_updateChannelsButton_clicked()
{
    ui->updateChannelsButton->setDisabled(true);
    ui->updateChannelsButton->setText("Updating Channels...");
    this->beginWebScraping();

}


void MainWindow::on_addToScanListBtn_clicked()
{
    // make sure setupListView is expected to contain a filtered list of channels
    if(this->setupState != MainWindow::SELECT_VALUE){
        return;
    }
    // ==== append the filtered list in setupListView to the scanListListView ====
    QAbstractItemModel* model = ui->setupListView->model();

    QAbstractItemModel* destModel = ui->scanListListView->model();
    if(destModel == nullptr){
        ui->scanListListView->setModel(new QStringListModel());
        destModel = ui->scanListListView->model();
    }

    // get list of proposed additions to scan list
    QVector<QString> listToAdd;
    for(int row = 0; row < model->rowCount(); row++){
        listToAdd.append(model->data(model->index(row, 0)).toString());
    }

    // get list of existing items in scan list
    QVector<QString> existingScanList;
    for(int row = 0; row < destModel->rowCount(); row++){
        existingScanList.append(destModel->data(destModel->index(row, 0)).toString());
    }

    // go through listToAdd, if item doesn't occur in existingScanList, add to scanList
    for(auto str : listToAdd){
        if(!existingScanList.contains(str)){
            QVariant var(str);
            int insertAt = destModel->rowCount();
            destModel->insertRow(insertAt);
            destModel->setData(destModel->index(insertAt, 0), var);
        }
    }

}

void MainWindow::on_beginScanBtn_clicked()
{
    // send the list of channels listed in scanListListView to the backend
    QAbstractItemModel* model = ui->scanListListView->model();
    if(model == nullptr || selected_county == nullptr){
        return;
    }
    QVector<Channel> channels;
    for(int row = 0; row < model->rowCount(); row++){
        Channel ch = selected_county->getChannelByString(model->data(model->index(row, 0)).toString());
        channels.push_back(ch);
    }
    emit setChannelScanList(channels); // set the scan list
    emit changeSearch(true); // trigger the start of scanning
}

void MainWindow::on_stopScanButton_clicked()
{
    emit changeSearch(false); // signal the stop of scanning
    ui->currentChannelInfoLbl->setText("----");
}

/**
 * @brief MainWindow::lswifiHandleData slot to handle data from the lswifi process
 */
void MainWindow::lswifiHandleData(){

    QString list = QString(lswifiProc->readAllStandardOutput());

    ui->wifiNetworksComboBox->clear();
    QStringList ssidList = list.split(',');
    ui->wifiNetworksComboBox->addItems(ssidList);
    for(auto s : ssidList){
      this->logMessage(s);
    }
}

/**
 * @brief MainWindow::scrapeSystemsHandleStdout slot to read standard output from the scrape systems process
 */
void MainWindow::scrapeSystemsHandleStdout(int exitCode, QProcess::ExitStatus exitStatus){
    if(exitStatus == QProcess::CrashExit){
        logMessage("Systems web scraper crashed.");
    }else{
        logMessage(QString("Systems web scraper done, exited with code %1").arg(exitCode));
    }
    QString output(scrapeSystemsProc->readAllStandardOutput());
    logMessage(output);
    std::istringstream iss(output.toStdString());
    QVector<QVector<QString>> csv = read_csv(iss);

    int name_ind = -1, id_ind = -1;
    for(int i = 0; i < csv[0].length(); i++){
        if(csv[0][i].contains("system name", Qt::CaseInsensitive))
            name_ind = i;
        else if(csv[0][i].contains("system id", Qt::CaseInsensitive))
            id_ind = i;
    }

    // append all the system data to the current county
    if(name_ind >= 0 && id_ind >= 0 && selected_county != nullptr){
        for(int i = 1; i < csv.length(); i++){
            QPair<QString, int> sys(csv[i][name_ind], csv[i][id_ind].toInt());
            if(!selected_county->systems.contains(sys)){
                selected_county->systems.push_back(sys);
            }
        }
    }

    QStringList modelList;

    for(QPair<QString, int> sys : selected_county->systems){
        modelList << QString("%1 : %2").arg(sys.second).arg(sys.first);
    }

    QAbstractItemModel *model = new QStringListModel(modelList);
    ui->setupListView->setModel(model);
}

/**
 * @brief MainWindow::scrapeChannelsHandleStdout slot to read from temp csv file when web scraper is finished
 * @param exitCode
 * @param exitStatus
 */
void MainWindow::scrapeChannelsHandleStdout(int exitCode, QProcess::ExitStatus exitStatus){
    this->webScrapeTimeoutTimer->stop();

    if(exitStatus == QProcess::CrashExit){
        logMessage("Web scraper crashed.");
    }else{
        qDebug() << QString("Web scraper done, exited with code %1").arg(exitCode);
        logMessage(QString("Web scraper done, exited with code %1").arg(exitCode));
    }
    QString tmpDirPath = QString("/var/lib/sdrapp/%1").arg("tmp");
    QString tempCSVPath = QString("%1/webscrape_results.csv").arg(tmpDirPath);
    QVector<QVector<QString>> csv = read_csv_file(tempCSVPath);
    QString fname;
    QVector<QString> protocolColumn(csv.length());

    if(selected_state == nullptr || selected_county == nullptr){
        return;
    }
    QDir countyDir(QString("/var/lib/sdrapp/%1/%2").arg(selected_state->name).arg(selected_county->name));
    if(!countyDir.exists()){
        logMessage(QString("Creating %1...").arg(countyDir.absolutePath()));
    }
    if(this->radio->getProtocol().compare("p25", Qt::CaseInsensitive) == 0){
        // P25 specific things
        fname = QString("%1/master_P25_talkgroups.csv").arg(countyDir.absolutePath());
        QString current_system_name = ui->setupListView->currentIndex().data().toString();
        QVector<QString> sysNameColumn(csv.length());
        QVector<QString> sysIdColumn(csv.length());
        sysNameColumn.fill(this->currentSystem.first);
        sysIdColumn.fill(QString("%1").arg(this->currentSystem.second));

        sysNameColumn[0] = "System Name";
        sysIdColumn[0] = "System ID";

        protocolColumn.fill("P25");
        add_csv_column(csv, sysNameColumn);
        add_csv_column(csv, sysIdColumn);

    }else{
        // just FM
        fname = QString("%1/master_FM_stations.csv").arg(countyDir.absolutePath());
        protocolColumn.fill("FM");
    }
    protocolColumn[0] = "Protocol";
    add_csv_column(csv, protocolColumn);
    QVector<Channel> newChannels = Radio::channelsFromCsv(csv);
    QVector<Channel> existingChannels = Radio::channelsFromCsv(read_csv_file(fname));
    QVector<Channel> combinedChannels = Radio::mergeChannels(existingChannels, newChannels); // combine new and existing channels

    if(selected_state != nullptr && selected_county != nullptr){
        // update channels with whatever information will be written to the master CSV file
        // no need to update from the actual file, we already have the info in a vector
        logMessage(QString("calling updateChannels for %1 county...").arg(selected_county->name));
        this->radio->updateChannels(selected_state->name, selected_county->name, combinedChannels);
    }

    // finally write out to corresponding file
    write_csv_file( Radio::channelsToCsv(combinedChannels) , fname );

    ui->updateChannelsButton->setText("Update Channels");
    ui->updateChannelsButton->setDisabled(false);

}

/**
 * @brief MainWindow::webScrapeTimeout slot to do things in the event of web scraping timing out
 */
void MainWindow::webScrapeTimeout(){
    this->webScrapeProc->kill(); // ensure process is ded
    logMessage("Web scraping process timed out.");
    // re enable update channels button
    ui->updateChannelsButton->setText("Update Channels");
    ui->updateChannelsButton->setDisabled(false);
}


void MainWindow::on_findWifiBtn_clicked()
{
    // use lswifi then populate combobox with SSID's
    QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();

    if(lswifiProc != nullptr && lswifiProc->state() == QProcess::Running){
        lswifiProc->kill();
    }

    if(sys.contains("LSWIFI_PATH")){
        // check for file existance
        QString path = sys.value("LSWIFI_PATH");
        if(QFile::exists(path) && QFile::permissions(path) & (QFileDevice::ExeGroup | QFileDevice::ExeUser | QFileDevice::ExeOther)){
            // exists and we can run it
            this->logMessage(QString("running %1...").arg(path));
            lswifiProc = new QProcess(this);
            connect(lswifiProc, &QProcess::readyReadStandardOutput, this, &MainWindow::lswifiHandleData);
            lswifiProc->start(QString("%1").arg(path), QStringList());

        }else{
            this->logMessage("lswifi.py not found.");
        }
    }else{
        this->logMessage("lswifi.py location unknown.");
    }
}

void MainWindow::on_connectToWifiBtn_clicked()
{
    // get current SSID from combobox

    // get password from passwdLineEdit

    // do things to connect to the wifi network
}

void MainWindow::on_p25Btn_clicked()
{
    // de-check the FM button
    ui->fmBtn->setChecked(false);

    // emit change protocol signal
    emit changeProtocol("p25");
}

void MainWindow::on_fmBtn_clicked()
{
    // de-check the FM button
    ui->p25Btn->setChecked(false);

    // emit change protocol signal
    emit changeProtocol("fm");
}

/**
 * @brief MainWindow::switchSetupState change contents of setupListView based on newState
 * @param newState state to switch to
 */
void MainWindow::switchSetupState(int newState){
    switch(newState){
    case MainWindow::SELECT_STATE:{
        QAbstractItemModel *model = new QStringListModel(this->radio->getStateNames());
        ui->setupListView->setModel(model);
        ui->setupStateBackButton->setDisabled(true);
        break;
    }case MainWindow::SELECT_COUNTY:{
        ui->setupStateBackButton->setDisabled(false);
        if(selected_state != nullptr){
            QAbstractItemModel *model = new QStringListModel(this->selected_state->getCountyNames());
            ui->setupListView->setModel(model);
            ui->setupStateNextButton->setDisabled(true);
            ui->setupStateBackButton->setDisabled(false);
            ui->setupStateLabel->setText("Select County");
        }
        break;
    }case MainWindow::SELECT_PROTOCOL:{
        ui->setupStateBackButton->setDisabled(false);
        // display protocols now
        if(this->selected_county != nullptr){
            QAbstractItemModel *model = new QStringListModel(this->radio->protocols);
            ui->setupListView->setModel(model);
            ui->setupStateNextButton->setDisabled(true);
            ui->setupStateBackButton->setDisabled(false);
            ui->setupStateLabel->setText("Select Type/Protocol");
        }
        break;
    }case MainWindow::SELECT_SYSTEM:{
        ui->setupStateBackButton->setDisabled(false);
        ui->setupStateNextButton->setDisabled(true);

        QStringList modelList;

        for(QPair<QString, int> sys : selected_county->systems){
            modelList << QString("%1 : %2").arg(sys.second).arg(sys.first);
        }

        QAbstractItemModel *model = new QStringListModel(modelList);
        ui->setupListView->setModel(model);

        ui->setupStateLabel->setText("Select P25 System");
        break;
    }case MainWindow::SELECT_SORT_BY:{
        ui->setupStateBackButton->setDisabled(false);
        ui->setupStateNextButton->setDisabled(true);
        QAbstractItemModel *model = new QStringListModel(QStringList() << "Group" << "Tag" << "All Channels");
        ui->setupListView->setModel(model);
        ui->setupStateLabel->setText("Select Filter Category");
        break;
    }case MainWindow::SELECT_VALUE:{
        QAbstractItemModel *model = nullptr;
        if(this->sortBy.compare("group", Qt::CaseInsensitive) == 0){
            QStringList groups;
            for(auto str : selected_county->getGroups()){
                groups << str;
            }
            model = new QStringListModel(groups);
        }else if(this->sortBy.compare("tag", Qt::CaseInsensitive) == 0){
            QStringList tags;
            for(auto str: selected_county->getTags()){
                tags << str;
            }
            model = new QStringListModel(tags);
        }else if(this->sortBy.compare("All Channels", Qt::CaseInsensitive) == 0){
            QStringList channels;
            for(auto ch : selected_county->channels){
                channels << ch.toString();
            }
            model = new QStringListModel(channels);
        }else{
            model = new QStringListModel(QStringList());
        }

        ui->setupListView->setModel(model);
        ui->setupStateLabel->setText(QString("Select %1").arg(this->sortBy));

        ui->setupStateBackButton->setDisabled(false);
        ui->setupStateNextButton->setDisabled(true);
    }default:{
        break;
    }

    };
    this->setupState = newState;
}


void MainWindow::on_setupStateNextButton_clicked()
{
    QString str = ui->setupListView->currentIndex().data().toString();
    logMessage(str);
    switch(this->setupState){
    case MainWindow::SELECT_STATE:{
        this->selected_state = this->radio->getStateByName(str);
        this->switchSetupState(MainWindow::SELECT_COUNTY);
        break;
    }case MainWindow::SELECT_COUNTY:{
        if(this->selected_county == nullptr || this->selected_county->name.compare(str) != 0){
            // new county selected
            this->selected_county = this->selected_state->getCountyByName(str);
        }
        switchSetupState(MainWindow::SELECT_PROTOCOL);
        break;
    }case MainWindow::SELECT_PROTOCOL:{
        if(this->selected_county == nullptr){
            break;
        }

        // need to do this regardless of whether additional scraping is necessary
        // gets current p25 systems for example, additional ones might come in shortly
        this->radio->updateChannelsFromFile(selected_state->name, selected_county->name);

        if(str.compare("p25", Qt::CaseInsensitive) == 0){
            // need to web scrape systems at this point
            QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();

            QString scrapeSystemsFile = "";
            if(sys.contains("SCRAPE_SYSTEMS_PATH")){
                scrapeSystemsFile = sys.value("SCRAPE_SYSTEMS_PATH");
            }else{
                scrapeSystemsFile = "web_scrape_systems.py";
            }
            logMessage("Scrape Systems script: " + scrapeSystemsFile);
            if(QFile::exists(scrapeSystemsFile)){// && QFile::permissions(scrapeSystemsFile) & (QFileDevice::ExeGroup | QFileDevice::ExeUser | QFileDevice::ExeOther)){
                scrapeSystemsProc->start("python3", QStringList() << scrapeSystemsFile << QString("%1").arg(selected_county->county_id));
            }else{
                logMessage("Scrape Systems script either not found or not executable.");
            }
            switchSetupState(MainWindow::SELECT_SYSTEM);
        }

        break;
    }case MainWindow::SELECT_SYSTEM:{

        switchSetupState(MainWindow::SELECT_SORT_BY);
        break;
    }case MainWindow::SELECT_SORT_BY:{
        switchSetupState(MainWindow::SELECT_VALUE);
        break;
    }case MainWindow::SELECT_VALUE:{
        QStringList channels;
        if(sortBy.compare("group", Qt::CaseInsensitive) == 0){
            for(auto ch : selected_county->getChannelsByGroup(this->sortValue)){
                channels << ch.toString();
            }
        }else if(sortBy.compare("tag", Qt::CaseInsensitive) == 0){
            for(auto ch : selected_county->getChannelsByTag(this->sortValue)){
                channels << ch.toString();
            }
        }else if(sortBy.compare("all channels", Qt::CaseInsensitive) == 0){
            // str is our channel string
            Channel ch = selected_county->getChannelByString(this->sortValue);
        }
        QAbstractItemModel *model = new QStringListModel(channels);
        ui->setupListView->setModel(model);
        break;
    }default:{
        switchSetupState(MainWindow::SELECT_STATE);
        break;
    }

    };
}

void MainWindow::on_setupStateBackButton_clicked()
{
    switch(this->setupState){
    case MainWindow::SELECT_STATE:{
        // make no changes
        break;
    }case MainWindow::SELECT_COUNTY:{
        ui->setupStateNextButton->setDisabled(true);
        switchSetupState(MainWindow::SELECT_STATE);
        break;
    }case MainWindow::SELECT_PROTOCOL:{
        ui->setupStateNextButton->setDisabled(true);
        switchSetupState(MainWindow::SELECT_COUNTY);
        break;
    }case MainWindow::SELECT_SYSTEM:{
        ui->setupStateNextButton->setDisabled(true);
        ui->updateChannelsButton->setDisabled(true);
        switchSetupState(MainWindow::SELECT_PROTOCOL);
        break;
    }case MainWindow::SELECT_SORT_BY:{
        ui->setupStateNextButton->setDisabled(true);
        ui->updateChannelsButton->setDisabled(true);
        switchSetupState(MainWindow::SELECT_SYSTEM);
        break;
    }case MainWindow::SELECT_VALUE:{
        ui->setupStateNextButton->setDisabled(true);
        switchSetupState(MainWindow::SELECT_SORT_BY);
        break;
    }default:{
        switchSetupState(MainWindow::SELECT_STATE);
        break;
    }

    };
}

void MainWindow::on_setupListView_clicked(const QModelIndex &index)
{
    QString str = index.data().toString();
    switch(this->setupState){
    case MainWindow::SELECT_STATE:{
        ui->setupStateNextButton->setDisabled(false);
        // click indicates that we've entered a state
        this->selected_state = this->radio->getStateByName(str);
        break;
    }case MainWindow::SELECT_COUNTY:{
        ui->setupStateNextButton->setDisabled(false);
        if(this->selected_state != nullptr)
            this->selected_county = this->selected_state->getCountyByName(str);

        break;
    }case MainWindow::SELECT_PROTOCOL:{
        if(this->radio->protocols.contains(str, Qt::CaseInsensitive)){
            logMessage("Changing protocol to " + str);
            if(str.compare("p25", Qt::CaseInsensitive) == 0){
                ui->setupStateNextButton->setDisabled(false);
                ui->p25Btn->click();
            }else if(str.compare("fm", Qt::CaseInsensitive) == 0){
                ui->fmBtn->click();
                ui->updateChannelsButton->setDisabled(false);
                ui->setupStateNextButton->setDisabled(true); // nowhere else to go
            }
        }
        break;
    }case MainWindow::SELECT_SYSTEM:{
        QStringList parts = str.split(':');
        this->currentSystem = QPair<QString, int>();
        this->currentSystem.first = parts[1];
        this->currentSystem.second = parts[0].remove(' ').toInt();
        logMessage(QString("Updating currentSystem to <%1,%2>").arg(this->currentSystem.first).arg(this->currentSystem.second));
        if(this->selected_county != nullptr){
            this->selected_county->currentSystem = parts[1];
            ui->updateChannelsButton->setDisabled(false);
        }
        ui->setupStateNextButton->setDisabled(false);

        break;
    }case MainWindow::SELECT_SORT_BY:{
        this->sortBy = str;
        ui->setupStateNextButton->setDisabled(false);
    }case MainWindow::SELECT_VALUE:{
        this->sortValue = str;
        ui->setupStateNextButton->setDisabled(false);
    }default:{
        // fix whatever is wrong
        break;
    }

    };
}
