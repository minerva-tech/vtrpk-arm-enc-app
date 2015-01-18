#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QSettings>

#include "registeredit.h"
#include "camregisteredit.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void static setLang(const QString& lang);

public slots:
    void setStartChecked(bool);
    void on_update_stat();

private slots:
    void on_EditDetectorSettings_clicked();

    void on_actionErase_Config_triggered();

    void on_actionEnglish_triggered();

    void on_actionRussian_triggered();

    void on_actionShowLog_triggered();

    void on_editCfg_clicked();

    void on_actionCOM1_triggered();
    void on_actionCOM2_triggered();
    void on_actionCOM3_triggered();
    void on_actionCOM4_triggered();

    void on_actionPort_Config_triggered();

    void on_ApplyDetectorSettings_clicked();

    void on_actionEdit_triggered();

/*    int setIRRange(int value);
    int getIRRange();*/
    void setShowThresholds(bool enabled);

    //void on_IRRange_valueChanged(int arg1);

    void on_CameraID_currentIndexChanged(int index);

    void on_actionCamera_triggered();

    void on_Thresholds_clicked(bool checked);

    void on_connectButton_clicked();

    void on_menuAbout_triggered();

    void on_actionVideo_Sensor_triggered();

private:
    static const int stat_renew_interval = 1000; //(msec) renew stat once a second

    void enableControls(bool);
    bool tryConnect();

    Ui::MainWindow*     ui;
    RegisterEdit*       m_regedit;
    CamRegisterEdit*    m_camregedit;
    QStandardItemModel  m_det_presets;
    QSettings           m_settings;
    QString             m_port_name;
    QTimer*             m_timer;
};

#endif // MAINWINDOW_H
