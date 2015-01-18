#ifndef VSENSORSETTINGS_H
#define VSENSORSETTINGS_H

#include <QDialog>

namespace Ui {
class VSensorSettings;
}

class VSensorSettings : public QDialog
{
    Q_OBJECT

public:
    explicit VSensorSettings(QWidget *parent = 0);
    ~VSensorSettings();

private:
    Ui::VSensorSettings *ui;
};

#endif // VSENSORSETTINGS_H
