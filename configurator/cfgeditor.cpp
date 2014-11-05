#include "ext_headers.h"
#include "pchbarrier.h"

#include <QDebug>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

#include "cfgeditor.h"
#include "ui_cfgeditor.h"
#include "progress_dialog.h"
#include "portconfig.h"

#include "comm.h"
#include "proto.h"

CfgEditor::CfgEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CfgEditor)
{
    ui->setupUi(this);
}

CfgEditor::~CfgEditor()
{
    delete ui;
}

void CfgEditor::on_downloadCfg_clicked()
{
    ProgressDialog progress(tr("Downloading encoder config"), QString(), 0, Client::get_enc_cfg_timeout.count(), this);
    QString cfg_str = QString::fromStdString(Client::GetEncCfg(&progress));

    this->ui->plainTextEdit->setPlainText(cfg_str);

	progress.close();
}

void CfgEditor::on_uploadCfg_clicked()
{
    Server::SendEncCfg(this->ui->plainTextEdit->toPlainText().toStdString());
}

void CfgEditor::on_loadCfg_clicked()
{
    QFileDialog d;

    d.setNameFilter("*.cfg");
    d.setWindowTitle("Load encoder config file");

    if (d.exec() == QDialog::Accepted && d.selectedFiles().size() == 1) {
        QFile cfgfile(d.selectedFiles().at(0));
        if (cfgfile.exists()) {
            cfgfile.open(QIODevice::ReadOnly);
            QTextStream stream(&cfgfile);
            QString str = stream.readAll();
            cfgfile.close();
            this->ui->plainTextEdit->setPlainText(str);
        }
    }
}

void CfgEditor::on_saveCfg_clicked()
{
    QFileDialog d;

    d.setNameFilter("*.cfg");
    d.setWindowTitle("Store encoder config file");
    d.setConfirmOverwrite(true);
    d.setLabelText(QFileDialog::Accept, "Save");

    if (d.exec() == QDialog::Accepted && d.selectedFiles().size() == 1) {
        QFile cfgfile(d.selectedFiles().at(0));
        cfgfile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        QTextStream stream(&cfgfile);
        stream << this->ui->plainTextEdit->toPlainText();
        cfgfile.close();
    }
}
