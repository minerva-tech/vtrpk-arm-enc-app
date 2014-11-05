#include "ext_headers.h"

#include "pchbarrier.h"

#include "ext_qt_headers.h"

#include "editdetpresets.h"
#include "detparams.h"
#include "ui_editdetpresets.h"

#include "defs.h"

EditDetPresets::EditDetPresets(QStandardItemModel* model, int currentItem, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditDetPresets),
    m_presets_model(model)
{
    assert(m_presets_model);

    ui->setupUi(this);
    ui->listView->setModel(m_presets_model);

	QModelIndex midx = model->index(currentItem, 0);

    ui->listView->setCurrentIndex(midx);

    connect(ui->Close, SIGNAL(clicked()), this, SLOT(accept()));
}

EditDetPresets::~EditDetPresets()
{
    emit(setPreset(ui->listView->currentIndex().row()));
    delete ui;
}

QList<QStandardItem*> EditDetPresets::generateNewPreset() const
{
    QList<QStandardItem*> preset;

    preset.push_back(new QStandardItem(tr("New Preset")));
    for (int i=0; i<static_cast<int>(sizeof(PRESET_DEFAULTS)/sizeof(PRESET_DEFAULTS[0])); i++) // This static_cast is dedicated to SPb AMD department
        preset.push_back(new QStandardItem(QString::number(PRESET_DEFAULTS[i])));

    return preset;
}

void EditDetPresets::on_AddPreset_clicked()
{
    assert(m_presets_model);
    m_presets_model->appendRow(generateNewPreset());
    DetParams d(m_presets_model, m_presets_model->rowCount()-1);
    d.exec();
}

void EditDetPresets::on_DeletePreset_clicked()
{
    m_presets_model->removeRow(ui->listView->currentIndex().row());
}

void EditDetPresets::on_Edit_clicked()
{
    DetParams d(m_presets_model, ui->listView->currentIndex().row());
    d.exec();
}
