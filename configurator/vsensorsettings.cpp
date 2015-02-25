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
