#ifndef REGISTEREDIT_H
#define REGISTEREDIT_H

#include <QDialog>

namespace Ui {
    class RegisterEdit;
}

class RegisterEdit : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterEdit(QWidget *parent = 0);
    ~RegisterEdit();

private slots:
    void on_getButton_clicked();

    void on_setButton_clicked();

  //  void on_Apply_clicked();

signals:
    void Play();
    void Stop();

private:
    Ui::RegisterEdit *ui;
};

#endif // REGISTEREDIT_H
