#ifndef DETPRESETSMODEL_H
#define DETPRESETSMODEL_H

#include <QStandardItemModel>

static const int DETECTOR_PRESETS_ITEM=1;

static const int N_VALUES=5;
static const int DEFAULTS[] = {55, 65, 65, 45, 22};

class DetPresetItem : public QStandardItem
{
public:
    DetPresetItem(const QString& text) :
        QStandardItem(text)
    {
        for (int i=0; i<N_VALUES; i++)
            m_values[i] = DEFAULTS[i];
    }

    int type() const { return UserType+DETECTOR_PRESETS_MODEL; }
    QStandardItem* clone() const { return new DetPresetItem(this); }

    QVariant data(int role) const;

private:
    int m_values[N_VALUES];
};

#endif // DETPRESETSMODEL_H
