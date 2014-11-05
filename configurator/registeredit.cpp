#include <QDebug>
#include "QMessageBox"
#include "ext_headers.h"
#include "comm.h"
#include "proto.h"
#include "portconfig.h"
#include "registeredit.h"
#include "ui_registeredit.h"

RegisterEdit::RegisterEdit(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegisterEdit)
{
    ui->setupUi(this);

    for (int i=0; i<0x44; i+=2)
        ui->addr->addItem("0x"+QString::number(i, 16));
}

RegisterEdit::~RegisterEdit()
{
    delete ui;
}

void AuxCb(uint8_t camera, const uint8_t* payload, int comment, uint16_t* val)
{
    if (Auxiliary::Type(payload) == Auxiliary::RegisterValType)
        *val = Auxiliary::RegisterVal(payload).val;
}

void RegisterEdit::on_getButton_clicked()
{
    Stop();

    int val = -1;

    Comm::instance().setCallback(boost::bind(AuxCb, _1, _2, _3, reinterpret_cast<uint16_t*>(&val)), 3);
    Server::SendCommand(Server::RequestRegister, ui->addr->currentIndex()*2);
	
	chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while (val == -1 && chrono::steady_clock::now() - start < Client::timeout)
        boost::this_thread::yield();

	if (val != -1)
		ui->val->setText(QString::number((uint16_t)val, 16));

    Comm::instance().setCallback(NULL, 3);

    Play();
}

void RegisterEdit::on_setButton_clicked()
{
    Stop();

	const uint16_t val = ui->val->text().toInt(NULL, 16);
	const int idx = ui->addr->currentIndex()*2;

    Auxiliary::SendRegisterVal(idx, val);

    Play();
}
