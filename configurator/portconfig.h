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
        m_name("COM4"),
        m_rate(38400),
        m_flow_control(false)
    {
        read();
    }

    static PortConfigSingleton& instance() { static PortConfigSingleton inst; /*read(inst);*/ return inst; }

    void set(const std::string& name, int rate, bool flow_control);

    std::string name() const;
    int rate() const;
    bool flow_control() const;

private:
    std::string m_name;
    int m_rate;
    bool m_flow_control;

    void read();
    void write() const;
};

#endif // PORTCONFIG_H
