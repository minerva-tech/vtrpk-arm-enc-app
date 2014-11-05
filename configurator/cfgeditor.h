#ifndef CFGEDITOR_H
#define CFGEDITOR_H

#include <QWidget>

namespace Ui {
    class CfgEditor;
}

class CfgEditor : public QWidget
{
    Q_OBJECT

public:
    explicit CfgEditor(QWidget *parent = 0);
    ~CfgEditor();

private slots:
    void on_loadCfg_clicked();

    void on_saveCfg_clicked();

    void on_downloadCfg_clicked();

    void on_uploadCfg_clicked();

private:
    Ui::CfgEditor *ui;
};

#endif // CFGEDITOR_H
