
#include "ext_headers.h"
#include "pchbarrier.h"

#include "ext_qt_headers.h"

#include <QMessageBox>
#include "mainwindow.h"
#include "defs.h"

#include "logview.h"
#include "errors.h"

int main(int argc, char *argv[])
{
	int ret = 0;

	try {
    QApplication a(argc, argv);

    QSettings settings("configurer.ini", QSettings::IniFormat);

    QString lang = settings.value(LanguageParam).toString();

    QTranslator appTranslator;
    appTranslator.load("configurer_" + lang, a.applicationDirPath());
    a.installTranslator(&appTranslator);

    MainWindow::setLang(lang);

    MainWindow w;

//    w.setLang(lang);

    w.show();

	log() << "Start";

	ret = a.exec();
	}
	catch(::exception& ex) {
		QApplication a(argc, argv); // i'm not sure if it's correct.
		QMessageBox message(QMessageBox::Critical,	QObject::tr("Fatal error"), QString::fromStdString(ex.text()));
		message.exec();
	}
	catch(...) {

	}

    return ret;
}
