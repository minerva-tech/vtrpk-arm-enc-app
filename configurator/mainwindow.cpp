#include "ext_headers.h"
#include "pchbarrier.h"

#include <QDebug>
#include <QTranslator>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include "progress_dialog.h"

#include "ext_qt_headers.h"

#include "mainwindow.h"
#include "editdetpresets.h"
#include "detparams.h"
#include "logview.h"
#include "qdswidget.h"
#include "cfgeditor.h"
#include "portconfig.h"
#include "vsensorsettings.h"
#include "ui_mainwindow.h"

#include "defs.h"

#include "comm.h"
#include "proto.h"
#include "auxiliary.h"

/*
static void print_comm_layout()
{
    log() << "m_callback:      " << (uint8_t*)&Comm::instance().m_callback[0] - (uint8_t*)&Comm::instance();
    log() << "m_thread:        " << (uint8_t*)&Comm::instance().m_thread - (uint8_t*)&Comm::instance();
    log() << "m_transmit_lock: " << (uint8_t*)&Comm::instance().m_transmit_lock - (uint8_t*)&Comm::instance();
    log() << "m_io_service:    " << (uint8_t*)&Comm::instance().m_io_service - (uint8_t*)&Comm::instance();
    log() << "m_port:          " << (uint8_t*)&Comm::instance().m_port - (uint8_t*)&Comm::instance();
    log() << "m_camera_id:     " << (uint8_t*)&Comm::instance().m_camera_id - (uint8_t*)&Comm::instance();
    log() << "m_int_count_lsb: " << (uint8_t*)&Comm::instance().m_in_count_lsb[0] - (uint8_t*)&Comm::instance();
    log() << "m_out_count_lsb: " << (uint8_t*)&Comm::instance().m_out_count_lsb[0] - (uint8_t*)&Comm::instance();
    log() << "m_out_buf:       " << (uint8_t*)&Comm::instance().m_out_buf - (uint8_t*)&Comm::instance();
    log() << "m_sending_in_pro:" << (uint8_t*)&Comm::instance().m_sending_in_progress - (uint8_t*)&Comm::instance();
    log() << "m_in_buf:        " << (uint8_t*)&Comm::instance().m_in_buf[0] - (uint8_t*)&Comm::instance();
    log() << "m_in_ff_idx:     " << (uint8_t*)&Comm::instance().m_in_ff_idx - (uint8_t*)&Comm::instance();
    log() << "m_cur:           " << (uint8_t*)&Comm::instance().m_cur - (uint8_t*)&Comm::instance();
    log() << "m_remains:       " << (uint8_t*)&Comm::instance().m_remains - (uint8_t*)&Comm::instance();

    log() << "m_buf:           " << (uint8_t*)&Comm::instance().m_out_buf.m_buf - (uint8_t*)&Comm::instance();
    log() << "m_buf_end:       " << (uint8_t*)&Comm::instance().m_out_buf.m_buf_end - (uint8_t*)&Comm::instance();

    log() << "m_w:             " << (uint8_t*)&Comm::instance().m_out_buf.m_w - (uint8_t*)&Comm::instance();
    log() << "m_r:             " << (uint8_t*)&Comm::instance().m_out_buf.m_r - (uint8_t*)&Comm::instance();

    log() << "m_empty:         " << (uint8_t*)&Comm::instance().m_out_buf.m_empty - (uint8_t*)&Comm::instance();
}*/

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_settings("configurer.ini", QSettings::IniFormat),
    m_regedit(NULL)
{
//    print_comm_layout();

    bool motion_enable = false;

    const bool connected = tryConnect(&motion_enable);
//    const bool connected = false;

    ui->setupUi(this);

    ui->CameraID->blockSignals(true);
    ui->CameraID->addItem("0");
    ui->CameraID->addItem("1");
    ui->CameraID->addItem("2");
    ui->CameraID->addItem("3");
    ui->CameraID->blockSignals(false);

    ui->CameraID->setCurrentIndex(Comm::instance().cameraID());

    enableControls(connected);
    ui->connectButton->setEnabled(!connected);

//    ui->enableMotion->setChecked(motion_enable);

    connect(ui->DSWidget, SIGNAL(setStartChecked(bool)), this, SLOT(setStartChecked(bool)));

//	setStartChecked(true);

    m_settings.beginGroup(PresetsGroup);

    //    QStringList keys = m_settings.allKeys();
    QStringList groups = m_settings.childGroups();

    //    const int len = m_settings.value(ColumnsCountParam).toInt();

    // see comment in on_EditDetectorSettings_clicked

    /*    foreach(const QString& key, keys) {
    if (key==ColumnsCountParam)
        continue;
    */
    foreach(const QString& group, groups) {
        m_settings.beginGroup(group);

        QList<QStandardItem*> row;
        row.push_back(new QStandardItem(group));

        // QMap? не, не слышал.
        row.push_back(new QStandardItem(m_settings.value(TimeThresParam).toString()));
        row.push_back(new QStandardItem(m_settings.value(SpatThresParam).toString()));
        row.push_back(new QStandardItem(m_settings.value(BotTempParam).toString()));
        row.push_back(new QStandardItem(m_settings.value(TopTempParam).toString()));
        row.push_back(new QStandardItem(m_settings.value(NoiseParam).toString()));

        m_det_presets.appendRow(row);

        m_settings.endGroup();

        /*
        QByteArray ba = m_settings.value(key).toByteArray();
        QDataStream ds(ba);

        QList<QStandardItem*> row;
        row.push_back(new QStandardItem(key));

        for (int c=1; c<len; c++) {
            row.push_back(new QStandardItem);
            QStandardItem* item = row.back();

            item->read(ds);
        }

        m_det_presets.appendRow(row);*/
    }

    m_settings.endGroup();

    ui->DetectorPresetsList->setModel(&m_det_presets);

    if (m_settings.value(LanguageParam) == "ru")
        ui->actionRussian->setChecked(true);
    else
        ui->actionEnglish->setChecked(true);

    if (m_settings.value(PortName).toString() == "COM1")
            ui->actionCOM1->setChecked(true);
    else if (m_settings.value(PortName).toString() == "COM2")
            ui->actionCOM2->setChecked(true);
    else if (m_settings.value(PortName).toString() == "COM3")
            ui->actionCOM3->setChecked(true);
    else if (m_settings.value(PortName).toString() == "COM4")
            ui->actionCOM4->setChecked(true);

//    ui->IRRange->setValue(getIRRange());

    setShowThresholds(false);

    ui->DSWidget->Play();

    m_timer = new QTimer(this);

    connect(m_timer, SIGNAL(timeout()), this, SLOT(on_update_stat()));

    m_timer->start(stat_renew_interval);
}

MainWindow::~MainWindow()
{
    Comm::instance().close();

    m_settings.sync();

    delete m_timer;
    delete m_regedit;
    delete ui;
}

void port_open()
{
    PortConfigSingleton& cfg = PortConfigSingleton::instance();

    if (!Comm::instance().open(cfg.addr(), cfg.port())) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setText(QObject::tr("Cannot establish connection with a device"));
        msg.exec();

        return;
    }
}

bool MainWindow::tryConnect(bool* motion_enable)
{
    ProgressDialog progress(tr("Establishing connection with a device"), QString(), 0, 5, this);

    int cam_id=3;

    bool connected = true;

    PortConfigSingleton& cfg = PortConfigSingleton::instance();

    if (!(Comm::instance().open(cfg.addr(), cfg.port()) && (cam_id = Client::Handshake(&progress, motion_enable))>=0)) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Cannot establish connection with a device"));
        msg.exec();
//        Server::SendCommand(Server::Start);
//        setStartChecked(true);
        connected = false;
    }

//    Comm::instance().close();

    progress.close();

    Comm::instance().setCameraID(cam_id);

    return connected;
}

void MainWindow::enableControls(bool enabled)
{
    ui->CameraID->setEnabled(enabled);
    ui->StartVideo->setEnabled(enabled);
    ui->StopVideo->setEnabled(enabled);
    ui->downloadROI->setEnabled(enabled);
    ui->clearROI->setEnabled(enabled);
    ui->uploadROI->setEnabled(enabled);
    ui->editCfg->setEnabled(enabled);
    ui->EditDetectorSettings->setEnabled(enabled);
    ui->ApplyDetectorSettings->setEnabled(enabled);
    ui->DetectorPresetsList->setEnabled(enabled);
    ui->actionEdit->setEnabled(enabled);
//    ui->actionCamera->setEnabled(enabled);
    ui->actionCamera->setEnabled(false); // Unfinished yet.
    ui->Thresholds->setEnabled(enabled);
    ui->Marker->setEnabled(enabled);
  //  ui->enableMotion->setEnabled(enabled);
}

void MainWindow::on_EditDetectorSettings_clicked()
{
    ui->DSWidget->Stop();

    EditDetPresets d(&m_det_presets, ui->DetectorPresetsList->currentIndex(), this);

    connect(&d, SIGNAL(setPreset(int)), ui->DetectorPresetsList, SLOT(setCurrentIndex(int)));

    d.exec();

    m_settings.beginGroup(PresetsGroup);

    m_settings.remove("");

    m_settings.setValue(ColumnsCountParam, m_det_presets.columnCount());

    for (int r=0; r<m_det_presets.rowCount(); r++) {
        // this way becomes shitty when parameter names, order and amount will be changed, but it produces more human-friendly
        // ini file. Probably there is better way but i don't know it. Well, ok, map with param name and value, and so on.
        // But i'll do it in a dumbest possible way.

        m_settings.beginGroup(m_det_presets.item(r, 0)->text());

        m_settings.setValue(TimeThresParam, m_det_presets.item(r, 1)->text());
        m_settings.setValue(SpatThresParam, m_det_presets.item(r, 2)->text());
        m_settings.setValue(BotTempParam, m_det_presets.item(r, 3)->text());
        m_settings.setValue(TopTempParam, m_det_presets.item(r, 4)->text());
        m_settings.setValue(NoiseParam, m_det_presets.item(r, 5)->text());

        m_settings.endGroup();

/*        QByteArray  *ba = new QByteArray;
        QDataStream *ds = new QDataStream(ba, QIODevice::WriteOnly);

        for (int c=1; c<m_det_presets.columnCount(); c++)
            if (m_det_presets.item(r, c)) {
                m_det_presets.item(r, c)->write(*ds);
//                *ds << m_det_presets.item(r, c); // WTF? DOESNT WORK! WHHHYYYYYYY?????
            }

        m_settings.setValue(m_det_presets.item(r, 0)->text(), *ba);

        delete ds;
        delete ba;*/
    }

    m_settings.endGroup();

	ui->DSWidget->Play();
}

void MainWindow::on_actionErase_Config_triggered()
{
    m_settings.clear();

    m_settings.sync();
}

void MainWindow::on_actionEnglish_triggered()
{
    m_settings.setValue(LanguageParam, "en");

    ui->actionRussian->setChecked(false);

    setLang("en");
}

void MainWindow::on_actionRussian_triggered()
{
    m_settings.setValue(LanguageParam, "ru");

    ui->actionEnglish->setChecked(false);

    setLang("ru");
}

void MainWindow::setLang(const QString& lang)
{
/*    QTranslator qtTranslator;
    qtTranslator.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    qApp->installTranslator(&qtTranslator);
*/
    QTranslator appTranslator;
//    qDebug() << appTranslator.load("configurer_" + lang, qApp->applicationDirPath());
    qApp->installTranslator(&appTranslator);
//    ui->retranslateUi(this);
}

void MainWindow::on_actionShowLog_triggered()
{
    LogView *log = new LogView;
    log->setAttribute(Qt::WA_DeleteOnClose);
    log->exec();
}

void MainWindow::on_editCfg_clicked()
{
    ui->DSWidget->Stop();

    CfgEditor *cfgeditor = new CfgEditor;
    cfgeditor->setAttribute(Qt::WA_DeleteOnClose);
    cfgeditor->show();
}

// You know how to do it properly? Tell me.
void MainWindow::on_actionCOM1_triggered()
{
    m_settings.setValue(PortName, "COM1");
    ui->actionCOM2->setChecked(false);
    ui->actionCOM3->setChecked(false);
    ui->actionCOM4->setChecked(false);
}

void MainWindow::on_actionCOM2_triggered()
{
    m_settings.setValue(PortName, "COM2");
    ui->actionCOM1->setChecked(false);
    ui->actionCOM3->setChecked(false);
    ui->actionCOM4->setChecked(false);
}

void MainWindow::on_actionCOM3_triggered()
{
    m_settings.setValue(PortName, "COM3");
    ui->actionCOM1->setChecked(false);
    ui->actionCOM2->setChecked(false);
    ui->actionCOM4->setChecked(false);
}

void MainWindow::on_actionCOM4_triggered()
{
    m_settings.setValue(PortName, "COM4");
    ui->actionCOM1->setChecked(false);
    ui->actionCOM2->setChecked(false);
    ui->actionCOM3->setChecked(false);
}

void MainWindow::on_actionPort_Config_triggered()
{
    PortConfig *portconfig = new PortConfig;
    portconfig->setAttribute(Qt::WA_DeleteOnClose);
    portconfig->show();
}

void MainWindow::on_ApplyDetectorSettings_clicked()
{
    ui->DSWidget->Stop();

    const int idx = ui->DetectorPresetsList->currentIndex();

    DetParams::storeParameters(&m_det_presets, idx);

    ui->DSWidget->Play();
}

void MainWindow::setStartChecked(bool b)
{
    ui->StartVideo->setChecked(b);
    ui->StopVideo->setChecked(!b);
}

void MainWindow::on_update_stat()
{
    ui->statText->setText(ui->DSWidget->getStatString());
}

void MainWindow::on_actionEdit_triggered()
{
    if (!m_regedit) {
        m_regedit = new RegisterEdit;
        connect(m_regedit, SIGNAL(Play()), ui->DSWidget, SLOT(Play()));
        connect(m_regedit, SIGNAL(Stop()), ui->DSWidget, SLOT(Stop()));
    }
    m_regedit->show();
}

void MainWindow::setShowThresholds(bool enable)
{
    const int regaddr = 0x42;

    qApp->setOverrideCursor(Qt::WaitCursor);

    ui->DSWidget->Stop();

    uint16_t regval = enable ? 0xC03F : 0x8080;

    Auxiliary::SendRegisterVal(regaddr, regval);

    ui->DSWidget->Play();

    qApp->setOverrideCursor(Qt::ArrowCursor);
}
/*
int MainWindow::getIRRange()
{
	return setIRRange(-1);
}

int MainWindow::setIRRange(int value)
{
	const int regaddr = 0x20;
	const int default_irrange = 2;

	qApp->setOverrideCursor(Qt::WaitCursor);

    ui->DSWidget->Stop();

    PortConfigSingleton& cfg = PortConfigSingleton::instance();

    if (!Comm::instance().open(cfg.name(), cfg.rate(), cfg.flow_control())) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("Cannot establish connection with a device"));
        msg.exec();

		qApp->setOverrideCursor(Qt::ArrowCursor);
        return 0;
    }

	int regval = -1;

    Comm::instance().setCallback(
    [&regval](uint8_t camera, const uint8_t* payload, int comment){
        if (Auxiliary::Type(payload) == Auxiliary::RegisterValType)
            regval = Auxiliary::RegisterVal(payload).val;
    }
        , Auxiliary::Port);

    Server::SendCommand(Server::RequestRegister, regaddr);

	chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while (regval == -1 && chrono::steady_clock::now() - start < Client::timeout)
        boost::this_thread::yield();

	int ir_range = default_irrange;

	Comm::instance().setCallback(NULL, 3);

	if (regval != -1) {

		ir_range = regval>>12;

		if (value >= 0) {
			regval &= 0x0fff;
			regval |= (value & 0xf) << 12;

			Auxiliary::SendRegisterVal(regaddr, regval);
		}
	}

	Comm::instance().transmit_and_close();

	ui->DSWidget->Play();

	qApp->setOverrideCursor(Qt::ArrowCursor);

	return ir_range;
}

void MainWindow::on_IRRange_valueChanged(int arg1)
{
    const int value = ui->IRRange->value();

    setIRRange(value);
}*/

void MainWindow::on_CameraID_currentIndexChanged(int index)
{
	if (index == Comm::instance().cameraID())
		return;

	ui->DSWidget->Stop();
	
	Server::SendID(index);
    
	Comm::instance().setCameraID(index);
	
	ui->DSWidget->Play();
}

void MainWindow::on_actionCamera_triggered()
{
    ui->DSWidget->Stop();

/*
    if (!m_camregedit)
        m_camregedit = new CamRegisterEdit;
*/
    CamRegisterEdit camregedit;

    camregedit.exec();

    ui->DSWidget->Play();
}

void MainWindow::on_Thresholds_clicked(bool checked)
{
    setShowThresholds(checked);
}

void MainWindow::on_connectButton_clicked()
{
    ui->DSWidget->Stop();
    bool enable_motion = false;
    const bool connected = tryConnect(&enable_motion);
    ui->CameraID->setCurrentIndex(Comm::instance().cameraID());
    enableControls(connected);
//    ui->enableMotion->setChecked(enable_motion);
    ui->connectButton->setEnabled(!connected);
}

void MainWindow::on_menuAbout_triggered()
{
    ui->DSWidget->Stop();

    ProgressDialog progress(tr("Downloading version info"), QString(), 0, Client::timeout.count(), this);
    QString version = QString::fromStdString(Client::GetVersionInfo(&progress));

    log() << version.toStdString();

    progress.close();

    QString configurator_ver = QString::fromLocal8Bit(__BUILD_NUMBER)+" ("+QString::fromLocal8Bit(__BUILD_DATE)+")";

    QMessageBox msg;
    msg.setIcon(QMessageBox::Information);
    msg.setText(tr("VRTPK firmware version\t")+version+"\n"+tr("Configurator build\t\t")+configurator_ver);
    msg.exec();

    return;
}

void MainWindow::on_actionVideo_Sensor_triggered()
{
    VSensorSettings d;
    d.exec();
}

void MainWindow::on_enableMotion_stateChanged(int arg1)
{
    ui->DSWidget->Stop();
    Server::SendCommand(Server::ToggleStreams, arg1 ? MOTION_ENABLE_BIT : 0);
    ui->DSWidget->Play();
}

void MainWindow::on_bitrateEdt_editingFinished()
{
    const int bitrate = (ui->bitrateEdt->text().toInt()+5)/10;

    Server::SendCommand(Server::SetBitrate, bitrate & 0xff, (bitrate >> 8) & 0xff);
}

void MainWindow::on_dropQueueBtn_clicked()
{
    Server::SendCommand(Server::BufferClear);
}
