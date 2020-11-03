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

    // populate states scroll area with state names
    QAbstractItemModel *model = new QStringListModel(this->radio->getStateNames());
    ui->statesListView->setModel(model);

    // empty model for county ListView
    ui->countiesListView->setModel(new QStringListModel());

    // populate sort-by box (sort by csv header fields)
    QStringList sortByFields;
    sortByFields << "Protocol" << "Tag" << "Mode" << "Talkgroup" << "Group";
    ui->sortByComboBox->insertItems(0, sortByFields);

    // channels ListView initial setup
    ui->channelsListView->setModel(new QStringListModel());

    // scan list ListView initial setup
    ui->scanListListView->setModel(new QStringListModel());

    // set p25 button to checked
    ui->p25Btn->click(); // check button and emit changeProtocol signal

    // ==== waterfall display setup ====
    //      freq fine adjust slider
    ui->freqFineAdjustSlider->setValue((ui->freqFineAdjustSlider->maximum() + ui->freqFineAdjustSlider->minimum())/2);

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
    this->updateFreqDisplay(status.frequency);

    // ==== scan list tab ====
    // construct text block
    QString channelInfo = QString("%1\r\n%2MHz\r\n%3dBm")
            .arg(status.channelName)
            .arg(status.frequency/1e6, 0, 'g', 4)
            .arg(status.signalPower, 0, 'g', 2);

    ui->currentChannelInfoLbl->setText(channelInfo); // display in Scan List tab

    // ==== waterfall tab ====
    ui->centerFreqLcdNumber->display(QString("%1").arg(status.frequency/1e6, 0, 'f', 1));


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

    double freq = this->getCenterFreqSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((freq - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((freq + bw/2)/1.0e6, 0, 'f', 4));
}

void MainWindow::on_frequencySlider_actionTriggered(int action)
{
    double freq = this->getCenterFreqSetpoint();
    emit changeFrequency(freq); // signal to radio the change frequency
    // this->updateFreqDisplay(this->radio->getCenterFreq());
    ui->centerFreqLcdNumber->display(QString("%1").arg(freq/1.0e6, 0, 'f', 1));
//    ui->activeFrequency->display(longs);

    double bw = this->getBandwidthSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((freq - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((freq + bw/2)/1.0e6, 0, 'f', 4));

}

void MainWindow::on_bandwidthSlider_actionTriggered(int action)
{
    double bw = getBandwidthSetpoint();
    //this->radio->setBandwidth(bw);
    emit changeBandwidth(bw);
    const QString s = QString("%1").arg(this->radio->getBandwidth()/1.0e3, 0, 'f', 1);
    ui->bandwidthLcdNumber->display(s);

    double freq = this->getCenterFreqSetpoint();
    ui->waterfallFreqLabelLeft->setText(QString("%1MHz").arg((freq - bw/2)/1.0e6, 0, 'f', 4));
    ui->waterfallFreqLabelRight->setText(QString("%1MHz").arg((freq + bw/2)/1.0e6, 0, 'f', 4));
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

void MainWindow::on_statesListView_activated(const QModelIndex &index)
{

}

void MainWindow::on_statesListView_clicked(const QModelIndex &index)
{
    // display the counties for this state
    QString str = index.data().toString();
    this->selected_state = this->radio->getStateByName(str);
    QAbstractItemModel *model = new QStringListModel(this->selected_state->getCountyNames());
    ui->countiesListView->setModel(model);
}

void MainWindow::on_countiesListView_clicked(const QModelIndex &index)
{
    QString str = index.data().toString();
    if(this->selected_county == nullptr || this->selected_county->name.compare(str) != 0){
        // new county, do things
        this->selected_county = this->selected_state->getCountyByName(str);

        // clear the category ListView by setting it to an empty model
        ui->channelsListView->setModel(new QStringListModel());

        // clear the scan list ListView by setting it to an empty model
        ui->scanListListView->setModel(new QStringListModel());

        // now see if we have some csv data for this state and county
        this->radio->updateChannelsFromFile(selected_state->name, selected_county->name);
    }



}

void MainWindow::on_updateChannelsButton_clicked()
{
    if(selected_state != nullptr && selected_county != nullptr){
        this->logMessage(QString("Updating channels for %2 County, %1").arg(selected_state->name).arg(selected_county->name));
    }else{
        return;
    }
    // call the web scraping program and process it's output
    // 1. construct URL
    // 2. call web scraping program and pass it the URL
    // 3. read in the csv output
    // 4. store in the appropriate directory
    // 5. update from the new csv files
    this->radio->updateChannelsFromFile(selected_state->name, selected_county->name);

}

void MainWindow::on_sortByComboBox_currentIndexChanged(const QString &arg1)
{
    // new grouping/sort-by thing
    // populate category combo box with available categories
    if(selected_county == nullptr){
        return;
    }
    if(arg1.compare("tag", Qt::CaseInsensitive) == 0){
        ui->categoryComboBox->clear();
        QStringList tags;
        for(auto str : selected_county->getTags()){
            tags << str;
        }
        ui->categoryComboBox->addItems(tags);
    }else if(arg1.compare("protocol", Qt::CaseInsensitive) == 0){
        ui->categoryComboBox->clear();
        QStringList protocols;
        for(auto str : selected_county->getProtocols()){
            protocols << str;
        }
        ui->categoryComboBox->addItems(protocols);
    }else if(arg1.compare("talkgroup", Qt::CaseInsensitive) == 0){
        ui->categoryComboBox->clear();
        QStringList talkgroups;
        for(auto str : selected_county->getTalkgroups()){
            talkgroups << str;
        }
        ui->categoryComboBox->addItems(talkgroups);
    }else if(arg1.compare("group", Qt::CaseInsensitive) == 0){
        ui->categoryComboBox->clear();
        QStringList groups;
        for(auto str : selected_county->getGroups()){
            groups << str;
        }
        ui->categoryComboBox->addItems(groups);
    }
}

void MainWindow::on_categoryComboBox_currentIndexChanged(const QString &arg1)
{
    // display channels fitting all the criteria
    QString sortBy = ui->sortByComboBox->currentText();
    logMessage("Sort by " + sortBy);
    QStringList channelStrings;

    if(selected_county == nullptr){
        return;
    }
    if(sortBy.compare("tag", Qt::CaseInsensitive) == 0){
        // show channels with tag matching arg1
        for(auto ch : selected_county->getChannelsByTag(arg1)){
            channelStrings << ch.toString();
        }
    }else if(sortBy.compare("protocol", Qt::CaseInsensitive) == 0){
        // show channels with protocol matching arg1
        for(auto ch : selected_county->getChannelsByProtocol(arg1)){
            channelStrings << ch.toString();
        }
    }else if(sortBy.compare("talkgroup", Qt::CaseInsensitive) == 0){
        // show channels with talkgrouup matching arg1
        for(auto ch : selected_county->getChannelsByTalkgroup(arg1)){
            channelStrings << ch.toString();
        }
    }else if(sortBy.compare("group", Qt::CaseInsensitive) == 0){
        // show channels with group matching arg1
        for(auto ch : selected_county->getChannelsByGroup(arg1)){
            channelStrings << ch.toString();
        }
    }

    QAbstractItemModel *model = new QStringListModel(channelStrings);
    ui->channelsListView->setModel(model);
}

void MainWindow::on_sortByComboBox_currentTextChanged(const QString &arg1)
{

}

void MainWindow::on_addToScanListBtn_clicked()
{
    // ==== append the filtered list in channelsListView to the scanListListView ====
    QAbstractItemModel* model = ui->channelsListView->model();

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

void MainWindow::on_findWifiBtn_clicked()
{
    // use lswifi then populate combobox with SSID's
    QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();

    if(sys.contains("LSWIFI_PATH")){
        // check for file existance
        QString path = sys.value("LSWIFI_PATH");
        if(QFile::exists(path) && QFile::permissions(path) & (QFileDevice::ExeGroup | QFileDevice::ExeUser | QFileDevice::ExeOther)){
            // exists and we can run it
            this->logMessage("running lswifi.py...");
            QProcess* lswifiProc = new QProcess(this);
            lswifiProc->start(QString(".%1").arg(path), QStringList());
            QString list = QString(lswifiProc->readAllStandardOutput());
            std::stringstream iss(list.toStdString());
            QVector<QVector<QString>> ssids = read_csv(iss);
            for(auto ssid : ssids[0]){
                this->logMessage(ssid);
                ui->wifiNetworksComboBox->addItem(ssid);
            }
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
    emit changeProtocol("P25");
}

void MainWindow::on_fmBtn_clicked()
{
    // de-check the FM button
    ui->p25Btn->setChecked(false);

    // emit change protocol signal
    emit changeProtocol("FM");
}
