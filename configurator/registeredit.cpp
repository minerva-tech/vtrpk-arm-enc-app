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
//   Comm::instance().tempCallback(boost::bind(AuxCb, _1, _2, _3, reinterpret_cast<uint16_t*>(&val)), 3);
    Server::SendCommand(Server::RequestRegister, ui->addr->currentIndex()*2);
	
	chrono::steady_clock::time_point start = chrono::steady_clock::now();
    while (val == -1 && chrono::steady_clock::now() - start < Client::timeout)
        boost::this_thread::yield();

	if (val != -1)
		ui->val->setText(QString::number((uint16_t)val, 16));

    Comm::instance().setCallback(NULL, 3);
//    Comm::instance().restoreCallback(3);

    Play();
}

void RegisterEdit::on_setButton_clicked()
{
 //   Stop();

	const uint16_t val = ui->val->text().toInt(NULL, 16);
	const int idx = ui->addr->currentIndex()*2;

    Auxiliary::SendRegisterVal(idx, val);

 //   Play();
}

void RegisterEdit::on_Apply_clicked()
{
  //   Stop();
    uint16_t data = 0x003a;
    uint16_t addr_wr = 0x0038;
    uint16_t addr_e2v_wr = 0x0280;
    uint16_t x;
    static const float Ga[] = {1., 1.5, 2., 3., 4., 6., 8., 8. };

    const uint8_t valGa = ui->analogGain->text().toInt(NULL,10);
    const uint8_t valGd = ui->digitalGain->text().toInt(NULL,10);
    const double Tint = ui->expo->text().toDouble();// in miliseconds

    assert(valGa<7);
    assert(valGd<256);

    x = (((uint8_t)valGa)<<8) | ((uint8_t)valGd);
    Auxiliary::SendRegisterVal(data, x);
    Auxiliary::SendRegisterVal(addr_wr, addr_e2v_wr|0x0011);

    uint32_t clocks = (Tint*0.001*24000000.0)/8;
    uint32_t lines = clocks / 112;
    uint32_t tacts = lines % 112;

    if(lines>65534) lines = 65534;
    if(tacts>255) tacts = 255;

    Auxiliary::SendRegisterVal(data, (uint16_t)lines);
    Auxiliary::SendRegisterVal(addr_wr, addr_e2v_wr|0x000E);

    Auxiliary::SendRegisterVal(data, (uint16_t)tacts);
    Auxiliary::SendRegisterVal(addr_wr, addr_e2v_wr|0x000F);

    char text_string[32];
    sprintf(text_string," %d",lines);
    ui->Lines->setText(text_string);
    sprintf(text_string," %d",tacts);
    ui->Tacts->setText(text_string);
    sprintf(text_string," %4.1f",Ga[valGa]);
    ui->ValGa->setText(text_string);
    sprintf(text_string," %5.3f", 1.0 + ((float)valGd*((15.875-1)/255.0)) );
    ui->ValGd->setText(text_string);

    //   Play();

}

