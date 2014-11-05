#ifndef CAMREGISTEREDIT_H
#define CAMREGISTEREDIT_H

#include <QDialog>

namespace Ui {
class CamRegisterEdit;
}

class CamRegisterEdit : public QDialog
{
    Q_OBJECT

public:
    explicit CamRegisterEdit(QWidget *parent = 0);
    ~CamRegisterEdit();

private slots:
    void on_clearButton_clicked();

    void on_sendButton_clicked();

private:
    Ui::CamRegisterEdit *ui;
};

#endif // CAMREGISTEREDIT_H
