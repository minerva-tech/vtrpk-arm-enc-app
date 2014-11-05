#include "ext_headers.h"

#include "pchbarrier.h"

#include "ext_qt_headers.h"
#include <QPushButton>

#include "detparams.h"
#include "ui_detparams.h"
#include "progress_dialog.h"
#include "portconfig.h"

#include "logview.h"
#include "comm.h"
#include "proto.h"

DetParams::DetParams(QStandardItemModel* model, int idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DetParams),
    m_idx(idx),
    m_model(model),
    m_base(10)
{
    ui->setupUi(this);

	QPushButton* loadFromDevice = new QPushButton(tr("Get"));
	ui->buttonBox->addButton(loadFromDevice, QDialogButtonBox::ActionRole);
	QPushButton* storeToDevice = new QPushButton(tr("Set"));
	ui->buttonBox->addButton(storeToDevice, QDialogButtonBox::ActionRole);

	connect(loadFromDevice, SIGNAL(pressed()), this, SLOT(loadParameters()));
	connect(storeToDevice,  SIGNAL(pressed()), this, SLOT(storeParameters()));

    connect(ui->TimeSlider, SIGNAL(valueChanged(int)), ui->TimeEdit, SLOT(setValue(int)));
    connect(ui->TimeEdit, SIGNAL(valueChanged(int)), ui->TimeSlider, SLOT(setValue(int)));
    connect(ui->SpatialSlider, SIGNAL(valueChanged(int)), ui->SpatialEdit, SLOT(setValue(int)));
    connect(ui->SpatialEdit, SIGNAL(valueChanged(int)), ui->SpatialSlider, SLOT(setValue(int)));
    connect(ui->TopTempSlider, SIGNAL(valueChanged(int)), ui->TopTempEdit, SLOT(setValue(int)));
    connect(ui->TopTempEdit, SIGNAL(valueChanged(int)), ui->TopTempSlider, SLOT(setValue(int)));
    connect(ui->BotTempSlider, SIGNAL(valueChanged(int)), ui->BotTempEdit, SLOT(setValue(int)));
    connect(ui->BotTempEdit, SIGNAL(valueChanged(int)), ui->BotTempSlider, SLOT(setValue(int)));
    connect(ui->NoiseSlider, SIGNAL(valueChanged(int)), ui->NoiseEdit, SLOT(setValue(int)));
    connect(ui->NoiseEdit, SIGNAL(valueChanged(int)), ui->NoiseSlider, SLOT(setValue(int)));

    connect(ui->TimeEdit, SIGNAL(valueChanged(int)), this, SLOT(setTimeValue(int)));
    connect(ui->SpatialEdit, SIGNAL(valueChanged(int)), this, SLOT(setSpatialValue(int)));
    connect(ui->TopTempEdit, SIGNAL(valueChanged(int)), this, SLOT(setTopTempValue(int)));
    connect(ui->BotTempEdit, SIGNAL(valueChanged(int)), this, SLOT(setBotTempValue(int)));
    connect(ui->NoiseEdit, SIGNAL(valueChanged(int)), this, SLOT(setNoiseValue(int)));

    m_mapper.setModel(m_model);
    m_mapper.addMapping(ui->NameEdit, 0);
    m_mapper.addMapping(ui->TimeEdit, 1);
    m_mapper.addMapping(ui->SpatialEdit, 2);
    m_mapper.addMapping(ui->TopTempEdit, 3);
    m_mapper.addMapping(ui->BotTempEdit, 4);
    m_mapper.addMapping(ui->NoiseEdit, 5);

    m_mapper.setCurrentIndex(m_idx);

    m_mapper.setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
}

DetParams::~DetParams()
{
    delete ui;
}

void DetParams::on_buttonBox_accepted()
{
    m_mapper.submit();
}

void DetParams::loadParameters()
{
	// stop();

    ProgressDialog progress(tr("Downloading MD parameters"), QString(), 0, 5, this);
	std::string str = Client::GetMDCfg(&progress);

	size_t pos = 0;
	if (str.size()) {
		size_t tmp;

		const uint32_t thres = QString::fromStdString(str.substr(pos, (tmp = str.find(' ', pos)) - pos)).toInt(); pos = tmp+1;
		m_model->item(m_idx, 1)->setText(QString::number(convertThres(thres, false)));

		const uint32_t spat_thres = QString::fromStdString(str.substr(pos, (tmp = str.find(' ', pos)) - pos)).toInt(); pos = tmp+1;
		m_model->item(m_idx, 2)->setText(QString::number(convertSpatThres(spat_thres, false)));

		const uint32_t top_t = QString::fromStdString(str.substr(pos, (tmp = str.find(' ', pos)) - pos)).toInt(); pos = tmp+1;
		m_model->item(m_idx, 3)->setText(QString::number(convertTopT(top_t, false)));

		const uint32_t bot_t = QString::fromStdString(str.substr(pos, (tmp = str.find(' ', pos)) - pos)).toInt(); pos = tmp+1;
		m_model->item(m_idx, 4)->setText(QString::number(convertBotT(bot_t, false)));

		const uint32_t noise = QString::fromStdString(str.substr(pos, (tmp = str.find(' ', pos)) - pos)).toInt();
		uint32_t out[3];
		convertNoise(out, noise, false);
		m_model->item(m_idx, 5)->setText(QString::number(out[0]));
	}

	m_mapper.revert();

	progress.close();
}

uint32_t DetParams::convertSpatThres(uint32_t val, bool toBoard)
{
	if (toBoard) {
		return val/100.0 * (13 - 1) + 1;
	} else {
		return (val-1)*100/(13-1)+0.5;
	}
}

#define THRESHOLD_BOTTOM_LIMIT ((double)1.0/65536.0)
#define THRESHOLD_TOP_LIMIT ((double)16384.0/65536.0)/* chinees ir sensor is 14-bit */

uint32_t DetParams::convertTopT(uint32_t val, bool toBoard)
{
	if (toBoard) {
        const double top_t = val/100.0 * (THRESHOLD_TOP_LIMIT-THRESHOLD_BOTTOM_LIMIT) + THRESHOLD_BOTTOM_LIMIT;
		const int top_t_i = top_t * (1<<18);
		return top_t_i;
	} else {
        return ((double)val/(1<<18)-THRESHOLD_BOTTOM_LIMIT)/(THRESHOLD_TOP_LIMIT-THRESHOLD_BOTTOM_LIMIT)*100+0.5;
	}
}

uint32_t DetParams::convertBotT(uint32_t val, bool toBoard)
{
	if (toBoard) {
        const double top_t = val/100.0 * (THRESHOLD_TOP_LIMIT-0.0) + 0.0;
		const int top_t_i = top_t * (1<<18);
		return top_t_i;
	} else {
        return ((double)val/(1<<18)-0.0)/(THRESHOLD_TOP_LIMIT-0.0)*100+0.5;
	}
}

/*
#if FPGA = A09
    #define FPGA_8BIT_PROCESSING
#else
    #undef FPGA_8BIT_PROCESSING
#endif

#ifdef FPGA_8BIT_PROCESSING
    #define NOISE_BOTTOM_LIMIT ((double)1.0/256.0)
    #define NOISE_TOP_LIMIT ((double)20.0/256.0)
#else
    #define NOISE_BOTTOM_LIMIT ((double)1.0/65536.0)
    #define NOISE_TOP_LIMIT ((double)100.0/65536.0)
#endif
*/

#define NOISE_BOTTOM_LIMIT ((double)1.0/65536.0)
//#define NOISE_TOP_LIMIT ((double)20.0/256.0)
#define NOISE_TOP_LIMIT ((double)100.0/65536.0)

#define THRESHOLD_SCALE_FACTOR ((double)256.0)


void DetParams::convertNoise(uint32_t out[3], uint32_t val, bool toBoard)
{
	if (toBoard) {
        const double noise = val/100.0 * (NOISE_TOP_LIMIT - NOISE_BOTTOM_LIMIT) + NOISE_BOTTOM_LIMIT;
		const double noise2 = noise*noise;
        const double inoise = (1./noise) / THRESHOLD_SCALE_FACTOR;

		out[0] = noise  * (1<<18);
		out[1] = noise2 * (1<<18);
		out[2] = inoise * (1<<18);
		
		return;
	} else {
	        out[0] = ((double)val/(1<<18)-NOISE_BOTTOM_LIMIT)/(NOISE_TOP_LIMIT - NOISE_BOTTOM_LIMIT)*100+0.5;
	}
}

#define THRESHOLD_KF_HI_VALUE (10.0)
#define THRESHOLD_KF_LO_VALUE (0.5)

uint32_t DetParams::convertThres(uint32_t val, bool toBoard)
{
    if (toBoard) {
        const double time_thres = ( val/100.0 * (THRESHOLD_KF_HI_VALUE - THRESHOLD_KF_LO_VALUE) + THRESHOLD_KF_LO_VALUE ) / THRESHOLD_SCALE_FACTOR;
        const int time_thres_i = time_thres * (1<<18);
        return time_thres_i;
    } else {
        return  (uint32_t)( (double)val * THRESHOLD_SCALE_FACTOR * 100.0 / THRESHOLD_KF_HI_VALUE / (double)(1<<18) + 0.5 );
    }
}

void DetParams::storeParameters(const QStandardItemModel* model, int idx)
{
    std::string str;

    str.append(QString::number(convertThres(model->item(idx, 1)->text().toInt(), true)).toStdString() + " ");
    str.append(QString::number(convertSpatThres(model->item(idx, 2)->text().toInt(), true)).toStdString() + " ");
    str.append(QString::number(convertTopT(model->item(idx, 3)->text().toInt(), true)).toStdString() + " ");
    str.append(QString::number(convertBotT(model->item(idx, 4)->text().toInt(), true)).toStdString() + " ");

    uint32_t noises[3];
    convertNoise(noises, model->item(idx, 5)->text().toInt(), true);

    str.append(QString::number(noises[0]).toStdString() + " ");
    str.append(QString::number(noises[1]).toStdString() + " ");
    str.append(QString::number(noises[2]).toStdString() + " ");

    Server::SendMDCfg(str);
}

void DetParams::storeParameters()
{
	// stop();
    m_mapper.submit();

    storeParameters(m_model, m_idx);
}

void DetParams::setTimeValue(int val)
{
    const int time_thres_i = convertThres(val, true);
    ui->TimeRaw->setText(QString::number(((double)time_thres_i / ((double)(1<<18)))*(THRESHOLD_SCALE_FACTOR)) + "(0x0" + QString::number(time_thres_i, 16)+")");
}

void DetParams::setSpatialValue(int val)
{
    const int spat_thres = convertSpatThres(val, true);
    ui->SpatialRaw->setText(QString::number(spat_thres, 10) + "(0x0" + QString::number(spat_thres, 16)+")");
}

void DetParams::setBotTempValue(int val)
{
    const int bot_t_i = convertBotT(val, true);
    ui->BotTempRaw->setText(QString::number((double)bot_t_i / (1<<18))+"(0x0" + QString::number(bot_t_i, 16)+")");
}

void DetParams::setTopTempValue(int val)
{
    const int top_t_i = convertTopT(val, true);
    ui->TopTempRaw->setText(QString::number((double)top_t_i / (1<<18))+"(0x0" + QString::number(top_t_i, 16)+")");
}

void DetParams::setNoiseValue(int val)
{
    uint32_t noise_i[3];
	convertNoise(noise_i, val, true);
    ui->NoiseRaw->setText(QString::number((double)noise_i[0] / (1<<18))+"(0x0" + QString::number(noise_i[0], 16)+")");
}
