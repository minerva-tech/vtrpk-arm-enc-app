#ifndef VSENSORSETTINGS_H
#define VSENSORSETTINGS_H

#include <QDialog>
#include <QAbstractButton>

namespace Ui {
class VSensorSettings;
}

class VSensorSettings : public QDialog
{
    Q_OBJECT

public:
    explicit VSensorSettings(QWidget *parent = 0);
    ~VSensorSettings();

private slots:
    void on_buttonBox_accepted();

    void on_Binning_currentIndexChanged(int index);

    void on_Apply_clicked();

    void on_AE_stateChanged(int ae);

private:
    Ui::VSensorSettings *ui;
};

#endif // VSENSORSETTINGS_H
