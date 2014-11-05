#include <fstream>

#include "portconfig.h"
#include "ui_portconfig.h"

PortConfig::PortConfig(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PortConfig)
{
    ui->setupUi(this);

    ui->portName->setText(PortConfigSingleton::instance().name().c_str());
    ui->baudRate->setText(QString::number(PortConfigSingleton::instance().rate()));
    ui->flowControl->setChecked(PortConfigSingleton::instance().flow_control());
}

PortConfig::~PortConfig()
{
    delete ui;
}

void PortConfig::on_buttonBox_accepted()
{
    const std::string name = ui->portName->text().toStdString();
    const int rate = ui->baudRate->text().toInt();
    const bool flow_control = ui->flowControl->isChecked();

    PortConfigSingleton::instance().set(name, rate, flow_control);
}


void PortConfigSingleton::set(const std::string& name, int rate, bool flow_control)
{
    m_name = name;
    m_rate = rate;
    m_flow_control = flow_control;

    write();
}

std::string PortConfigSingleton::name() const
{
    return m_name;
}
int PortConfigSingleton::rate() const
{
    return m_rate;
}

bool PortConfigSingleton::flow_control() const
{
    return m_flow_control;
}

void PortConfigSingleton::read()
{
    std::ifstream ofs((qApp->applicationDirPath()+"/port.cfg").toStdString());

    if (!ofs.is_open())
        return;

    std::string name;
    int rate;
    bool flow_control;

    ofs >> name;
    ofs >> rate;
    ofs >> flow_control;

    set(name, rate, flow_control);
}

void PortConfigSingleton::write() const
{
    std::ofstream ofs((qApp->applicationDirPath()+"/port.cfg").toStdString(), std::ios_base::trunc);

    ofs << m_name << std::endl;
    ofs << m_rate << std::endl;
    ofs << m_flow_control << std::endl;
}

