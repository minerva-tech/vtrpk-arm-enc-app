#ifndef PORTCONFIG_H
#define PORTCONFIG_H

#include <QDialog>

namespace Ui {
    class PortConfig;
}

class PortConfig : public QDialog
{
    Q_OBJECT

public:
    explicit PortConfig(QWidget *parent = 0);
    ~PortConfig();

private slots:
    void on_buttonBox_accepted();

private:
    Ui::PortConfig *ui;
};

class PortConfigSingleton
{
public:
    PortConfigSingleton() :
        m_addr("127.0.0.1"),
        m_port(20001)
    {
        read();
    }

    static PortConfigSingleton& instance() { static PortConfigSingleton inst; /*read(inst);*/ return inst; }

    void set(const std::string& addr, unsigned short port);

    std::string addr() const;
    unsigned short port() const;

private:
    std::string m_addr;
    unsigned m_port;

    void read();
    void write() const;
};

#endif // PORTCONFIG_H
