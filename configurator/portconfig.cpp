#include <fstream>

#include "portconfig.h"
#include "ui_portconfig.h"

PortConfig::PortConfig(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PortConfig)
{
    ui->setupUi(this);

/*    ui->portName->setText(PortConfigSingleton::instance().name().c_str());
    ui->baudRate->setText(QString::number(PortConfigSingleton::instance().rate()));
    ui->flowControl->setChecked(PortConfigSingleton::instance().flow_control());*/
}

PortConfig::~PortConfig()
{
    delete ui;
}

void PortConfig::on_buttonBox_accepted()
{
/*    const std::string name = ui->portName->text().toStdString();
    const int rate = ui->baudRate->text().toInt();
    const bool flow_control = ui->flowControl->isChecked();

    PortConfigSingleton::instance().set(name, rate, flow_control);*/
}


void PortConfigSingleton::set(const std::string& addr, unsigned short port)
{
    m_addr = addr;
    m_port = port;

    write();
}

std::string PortConfigSingleton::addr() const
{
    return m_addr;
}
unsigned short PortConfigSingleton::port() const
{
    return m_port;
}

void PortConfigSingleton::read()
{
    std::ifstream ofs((qApp->applicationDirPath()+"/port.cfg").toStdString());

    if (!ofs.is_open())
        return;

    std::string addr;
    unsigned short port;

    ofs >> addr;
    ofs >> port;

    set(addr, port);
}

void PortConfigSingleton::write() const
{
    std::ofstream ofs((qApp->applicationDirPath()+"/port.cfg").toStdString(), std::ios_base::trunc);

    ofs << m_addr << std::endl;
    ofs << m_port << std::endl;
}

