#include "ext_headers.h"

#include "pchbarrier.h"

#include "QScrollBar"

#include "ext_qt_headers.h"

#include "logview.h"
#include "ui_logview.h"

LogView::LogView(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogView)
{
    ui->setupUi(this);

    std::ifstream t((QApplication::applicationDirPath()+"/log.txt").toStdString());

    if (t.is_open()) {

        std::string str;

        t.seekg(0, std::ios::beg);
        const size_t size = t.tellg();
        str.reserve(size);

        t.seekg(0, std::ios::beg);

        str.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

        ui->textEdit->setPlainText(QString::fromStdString(str));

//        ui->textEdit->verticalScrollBar()->setValue(ui->textEdit->verticalScrollBar()->maximum());

        ui->textEdit->moveCursor(QTextCursor::End);
        ui->textEdit->ensureCursorVisible();
    }

//	Log::setEditView(ui->textEdit);
}

LogView::~LogView()
{
//	Log::setEditView(NULL);

    delete ui;
}
