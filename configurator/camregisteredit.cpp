#include "ext_headers.h"

#include "pchbarrier.h"

#include "camregisteredit.h"
#include "ui_camregisteredit.h"

#include "comm.h"
#include "proto.h"

void AuxCb(uint8_t camera, const uint8_t* payload, int comment, QTextEdit* text_edit)
{
    if (camera != Comm::instance().cameraID())
        return;

    if (Auxiliary::Type(payload) != Auxiliary::CameraRegisterValType)
        return;

    Auxiliary::CameraRegisterValData cam_data = Auxiliary::CameraRegisterVal(payload);
    QByteArray data = QByteArray::fromRawData(reinterpret_cast<const char*>(&cam_data.val[0]), Auxiliary::Size(payload));
    text_edit->append(data.toHex());
}

CamRegisterEdit::CamRegisterEdit(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CamRegisterEdit)
{
    ui->setupUi(this);
    Comm::instance().setCallback(boost::bind(AuxCb, _1, _2, _3, ui->textEdit), 3);
}

CamRegisterEdit::~CamRegisterEdit()
{
    Comm::instance().setCallback(NULL, 3);
    delete ui;
}

void CamRegisterEdit::on_clearButton_clicked()
{
    ui->textEdit->clear();
}

void send_command(const QString& cmd_str, const QString& arg_str)
{
    QByteArray cmd = QByteArray::fromHex(cmd_str.toLatin1());
    QByteArray arg = QByteArray::fromHex(arg_str.toLatin1());

    if (cmd.size() != 1)
        return;

    std::vector<uint8_t> p;
    p.reserve(10+arg.size());

    p.push_back(0x6e); // Process code
    p.push_back(0x00); // Status byte
    p.push_back(0x00); // Reserved
    p.push_back(cmd[0]); // Function
    const uint16_t byte_count = arg.size();
    p.push_back(byte_count>>8); // byte count msb
    p.push_back(byte_count&0xff); // byte count lsb

    //boost::crc_ccitt_type crc;
    boost::crc_optimal<16, 0x1021, 0, 0, false, false>  crc;
    crc.process_bytes(&p[0], p.size());
    const uint16_t crc1 = crc.checksum();

    p.push_back(crc1>>8);
    p.push_back(crc1&0xff);

    for(int i=0; i<arg.size(); i++)
        p.push_back(arg[i]);

    crc.reset();
    crc.process_bytes(&p[0], p.size());

    boost::crc_optimal<16, 0x1021, 0, 0, false, false>  crc2_;
    crc2_.process_bytes(&p[0], p.size());

    const uint16_t crc2 = crc2_.checksum();

    p.push_back(crc2>>8);
    p.push_back(crc2&0xff);

    Auxiliary::SendCameraRegisterVal(&p[0], p.size());
}

void CamRegisterEdit::on_sendButton_clicked()
{
    send_command(ui->commandEdit->text(), ui->argumentEdit->text());
}
