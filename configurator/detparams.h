#ifndef DETPARAMS_H
#define DETPARAMS_H

#include <QDialog>
#include <QDataWidgetMapper>
#include <QStandardItemModel>

typedef unsigned int uint32_t;

namespace Ui {
class DetParams;
}

class DetParams : public QDialog
{
    Q_OBJECT
    
public:
    explicit DetParams(QStandardItemModel* model, int idx, QWidget *parent = 0);
    ~DetParams();

    static void storeParameters(const QStandardItemModel* model, int idx);
    
public slots:
    void on_buttonBox_accepted();

    void loadParameters();
    void storeParameters();

    void setTimeValue(int);
    void setSpatialValue(int);
    void setBotTempValue(int);
    void setTopTempValue(int);
    void setNoiseValue(int);

private:
    static uint32_t convertThres(uint32_t val, bool toBoard);
    static uint32_t convertSpatThres(uint32_t val, bool toBoard);
    static uint32_t convertTopT(uint32_t val, bool toBoard);
    static uint32_t convertBotT(uint32_t val, bool toBoard);
    static void convertNoise(uint32_t out[3], uint32_t val, bool toBoard);

    const double m_base;

    Ui::DetParams *ui;

    QDataWidgetMapper m_mapper;

    const int m_idx;

    QStandardItemModel* m_model;
};

#endif // DETPARAMS_H
