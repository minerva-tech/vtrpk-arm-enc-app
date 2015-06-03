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

    ui->Binning->insertItems(0, QStringList() << tr("No") << "x/1" << "x/2" << "x/4");\
}

VSensorSettings::~VSensorSettings()
{
    delete ui;
}

void VSensorSettings::on_buttonBox_accepted()
{
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

    Auxiliary::SendVideoSensorSettings(set);
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

    const uint8_t valGa = ui->analogGain->text().toInt(NULL,10);
    const uint8_t valGd = ui->digitalGain->text().toInt(NULL,10);
    const double Tint = ui->expo->text().toDouble();// in miliseconds

    assert(valGa<7);
    assert(valGd<256);

    x = (((uint8_t)valGa)<<8) | ((uint8_t)valGd);
    Auxiliary::SendRegisterVal(data, x);
    Auxiliary::SendRegisterVal(addr_wr, addr_e2v_wr|0x0011);

    uint32_t clocks = (Tint*0.001*24000000.0)/8;
    uint32_t lines = clocks / 112;
    uint32_t tacts = lines % 112;

    if(lines>65534) lines = 65534;
    if(tacts>255) tacts = 255;

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

    //   Play();

}
