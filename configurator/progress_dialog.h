#ifndef PROGRESS_DIALOG_H
#define PROGRESS_DIALOG_H

#include <QObject>
#include <QApplication>
#include <QDateTime>
#include <QProgressDialog>
#include "iobserver.h"

class ProgressDialog : public QProgressDialog, public IObserver
{
	Q_OBJECT

public:
	ProgressDialog(const QString &labelText, const QString &cancelButtonText,
				int minimum, int maximum, QWidget *parent = 0, Qt::WindowFlags flags = 0) :
		QProgressDialog(labelText, cancelButtonText, minimum, maximum, parent, flags),
		v(0),
		m_time(QDateTime::currentMSecsSinceEpoch())
	{
		setWindowModality(Qt::WindowModal);
		setFixedSize(200,70);
		open();
		setValue(0);
	}

	ProgressDialog::~ProgressDialog()
	{
		close();
	}

	void progress(int val=0) {
		if (!val) {
			const int new_v = (QDateTime::currentMSecsSinceEpoch() - m_time)/1000;
			if (new_v != v) {
				setValue(v = new_v);
				qApp->processEvents();
			}
		} else {
			if (val != v) {
				v = val;
				setValue(v);
				qApp->processEvents();
			}
		}		
	}

private:
	int v;
	qint64 m_time;
};

#endif
