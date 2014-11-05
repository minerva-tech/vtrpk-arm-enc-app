#ifndef EDITDETPRESETS_H
#define EDITDETPRESETS_H

#include <QDialog>
#include <QModelIndex>
#include <QStandardItemModel>

namespace Ui {
class EditDetPresets;
}

class EditDetPresets : public QDialog
{
    Q_OBJECT
    
public:
    explicit EditDetPresets(QStandardItemModel* model, int currentItem, QWidget *parent = 0);
    ~EditDetPresets();

    void setModel(class QStandardItemModel* model);

private slots:
    void on_AddPreset_clicked();

    void on_DeletePreset_clicked();

    void on_Edit_clicked();

signals:
    void setPreset(int);

private:
    Ui::EditDetPresets *ui;

    QStandardItemModel *m_presets_model;

    QList<QStandardItem*> generateNewPreset() const;
};

#endif // EDITDETPRESETS_H
