#include "vsensorsettings.h"
#include "ui_vsensorsettings.h"
#include "logview.h"
#include "ext_headers.h"
#include "comm.h"
#include "proto.h"

VSensorSettings::VSensorSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VSensorSettings)
{
    ui->setupUi(this);

    ui->Binning->insertItems(0, QStringList() << tr("No") << "x/1" << "x/2" << "x/4");
    ui->analogGain->setText("0");
    ui->digitalGain->setText("0");
    ui->expo->setText("0.9");
    //
    {
        Stop();

        int val = -1;

        Comm::instance().setCallback(boost::bind(AuxCb, _1, _2, _3, reinterpret_cast<uint16_t*>(&val)), 3);
        Server::SendCommand(Server::RequestVSensorConfig);

        chrono::steady_clock::time_point start = chrono::steady_clock::now();
        while (val == -1 && chrono::steady_clock::now() - start < Client::timeout)
            boost::this_thread::yield();

        //Vova! use it as example
        //if (val != -1)
        //    ui->val->setText(QString::number((uint16_t)val, 16));

        Comm::instance().setCallback(NULL, 3);

        Play();
    }
    //
}

VSensorSettings::~VSensorSettings()
{
    delete ui;
}

void VSensorSettings::on_buttonBox_accepted()
{
    /*
    Auxiliary::VideoSensorResolutionData res;

    res.src_w = ui->SensorResX->text().toInt();
    res.src_h = ui->SensorResY->text().toInt();
    res.dst_w = ui->TargetResX->text().toInt();
    res.dst_h = ui->TargetResY->text().toInt()/2;

    Auxiliary::SendVideoSensorResolution(res);

    Auxiliary::VideoSensorSettingsData set;

    set.binning = ui->Binning->currentIndex();
    set.fps_divider = ui->FramerateDivider->text().toInt();
    set.pixel_correction = ui->PixelCorrection->isChecked() ? 1 : 0;
    set.ten_bit_compression = ui->Compression->isChecked() ? 1 : 0;
    set.ae_enable = ui->AE->isChecked() ? 1 : 0;

    Auxiliary::SendVideoSensorSettings(set);
    */
    on_Apply_clicked();
}

void VSensorSettings::on_Binning_currentIndexChanged(int index)
{
    if (index == 0) {
        ui->SensorResX->setText("1280");
        ui->SensorResY->setText("1024");

        ui->PixelCorrection->setEnabled(true);
    } else {
        ui->SensorResX->setText("640");
        ui->SensorResY->setText("512");

        ui->PixelCorrection->setChecked(false);
        ui->PixelCorrection->setEnabled(false);
    }
}


void VSensorSettings::on_Apply_clicked()
{
  //   Stop();
    uint16_t data = 0x003a;
    uint16_t addr_wr = 0x0038;
    uint16_t addr_e2v_wr = 0x0280;
    uint16_t x;
    static const float Ga[] = {1., 1.5, 2., 3., 4., 6., 8., 8. };

    uint8_t valGa = ui->analogGain->text().toInt(NULL,10);
    uint16_t valGd = ui->digitalGain->text().toInt(NULL,10);
    double Tint = ui->expo->text().toDouble();// in miliseconds

    if(valGa>6){
        valGa=6;
        ui->analogGain->setText("6");
    }
    if(valGd>255){
        valGd=255;
        ui->digitalGain->setText("255");
    }
    if(Tint<0.0081){
        Tint=0.0081;
        ui->expo->setText("0.0015");
    }
    if(Tint>((double)(65534*112*8 + 255*8))/24000.0){
        Tint=((double)(65534*112*8 + 255*8))/24000.0;
        ui->expo->setText("2446.688");
    }

    x = (((uint8_t)valGa)<<8) | ((uint8_t)valGd);
    Auxiliary::SendRegisterVal(data, x);
    Auxiliary::SendRegisterVal(addr_wr, addr_e2v_wr|0x0011);

    /* 112*8 tacts of 24MHz clock are in line */
    double clocks = (Tint*0.001*24000000.0)/8.0;
    uint32_t lines = (uint32_t)floor(clocks / 112.0);
    uint32_t tacts = (uint32_t)ceil(clocks - (double)(lines * 112));

    if(lines>65534) lines = 65534;
    if(tacts>255) tacts = 255;
    if(lines==0)
        if(tacts<25)
            tacts = 25;

    Auxiliary::SendRegisterVal(data, (uint16_t)lines);
    Auxiliary::SendRegisterVal(addr_wr, addr_e2v_wr|0x000E);

    Auxiliary::SendRegisterVal(data, (uint16_t)tacts);
    Auxiliary::SendRegisterVal(addr_wr, addr_e2v_wr|0x000F);

    char text_string[32];
    sprintf(text_string," %d",lines);
    ui->Lines->setText(text_string);
    sprintf(text_string," %d",tacts);
    ui->Tacts->setText(text_string);
    sprintf(text_string," %4.1f",Ga[valGa]);
    ui->ValGa->setText(text_string);
    sprintf(text_string," %5.3f", 1.0 + ((float)valGd*((15.875-1)/255.0)) );
    ui->ValGd->setText(text_string);

    /* resolution, ae, binning, frame divider, compression */
    Auxiliary::VideoSensorResolutionData res;

    res.src_w = ui->SensorResX->text().toInt();
    res.src_h = ui->SensorResY->text().toInt();
    res.dst_w = ui->TargetResX->text().toInt();
    res.dst_h = ui->TargetResY->text().toInt()/2;

    Auxiliary::SendVideoSensorResolution(res);

    Auxiliary::VideoSensorSettingsData set;

    set.binning = ui->Binning->currentIndex();
    set.fps_divider = ui->FramerateDivider->text().toInt();
    set.pixel_correction = ui->PixelCorrection->isChecked() ? 1 : 0;
    set.ten_bit_compression = ui->Compression->isChecked() ? 1 : 0;
    set.ae_enable = ui->AE->isChecked() ? 1 : 0;

    Auxiliary::SendVideoSensorSettings(set);

}

void VSensorSettings::on_AE_stateChanged(int ae)
{
    if (ae == 0) {
        ui->expo->setEnabled(true);
        ui->digitalGain->setEnabled(true);
        ui->analogGain->setEnabled(true);
        ui->Lines->setEnabled(true);
        ui->Tacts->setEnabled(true);
        ui->ValGa->setEnabled(true);
        ui->ValGd->setEnabled(true);
    } else {
        ui->expo->setEnabled(false);
        ui->digitalGain->setEnabled(false);
        ui->analogGain->setEnabled(false);
        ui->Lines->setEnabled(false);
        ui->Tacts->setEnabled(false);
        ui->ValGa->setEnabled(false);
        ui->ValGd->setEnabled(false);
    }
}
