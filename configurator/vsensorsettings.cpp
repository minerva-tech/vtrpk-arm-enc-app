#include "vsensorsettings.h"
#include "ui_vsensorsettings.h"

VSensorSettings::VSensorSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VSensorSettings)
{
    ui->setupUi(this);

    ui->Binning->insertItems(0, QStringList() << tr("No") << "x/1" << "x/2" << "x/4");
}

VSensorSettings::~VSensorSettings()
{
    delete ui;
}
