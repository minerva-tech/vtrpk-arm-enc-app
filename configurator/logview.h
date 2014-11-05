#ifndef LOG_H
#define LOG_H

#pragma warning(disable : 4995)

#ifndef Q_MOC_RUN
#include <boost/date_time.hpp>
#endif

#include <QApplication>
#include <QTextEdit>
#include <QTextStream>
#include <QDialog>

class Log {
	friend Log& log();
public:
    Log() : m_ofs((QApplication::applicationDirPath()+"/log.txt").toStdString().c_str(), std::ios_base::app), m_edit(NULL),	m_sev_view(Error), m_sev_stream(Debug)
	{ m_ofs << "[" << boost::gregorian::day_clock::local_day() << "]" << std::endl; }

	enum Severity {
		Dump = 0,
		Debug,
		Warning,
		Error,
		Critical
	};

	class PrintLine {
		friend class Log;
	
	public:
		template <typename T>
		PrintLine& operator << (const T& v) {
			if (m_edit && m_write_to_view) {
				QTextStream stream(&m_str, QIODevice::Append);
				stream << v;
			}
			if (m_write_to_stream)
				m_ofs << v;
			return *this;
		}

//		template <>
        PrintLine& operator << /*<std::string>*/(const std::string& v) { // qt shit
			if (m_edit && m_write_to_view) {
				QTextStream stream(&m_str, QIODevice::Append);
				stream << QString::fromStdString(v);
			}
			if (m_write_to_stream)
				m_ofs << v;
			return *this;
		}

		~PrintLine() { 
			if (m_edit && m_write_to_view)
				m_edit->append(m_str);
			if (m_write_to_stream)
				m_ofs << std::endl;
		}

	private:
		template <typename T>
		PrintLine(const T& v, std::ofstream& ofs, QTextEdit* edit, bool write_to_view, bool write_to_stream) : 
			m_ofs(ofs), 
			m_edit(edit), 
			m_write_to_view(write_to_view),
			m_write_to_stream(write_to_stream)
		{
			std::ostringstream time;
			time << "[" << boost::posix_time::second_clock::local_time().time_of_day() << "] ";
			*this << time.str();
			*this << v;
		}

		std::ofstream& m_ofs;
		QTextEdit* m_edit;
		QString m_str;

		const bool m_write_to_view;
		const bool m_write_to_stream;
	};

	template <typename T>
	PrintLine operator<< (const T& v) {
		return PrintLine(v, m_ofs, m_edit, Warning >= m_sev_view, Warning >= m_sev_stream);
	}

	template <>
	PrintLine operator << <Severity> (const Severity& s);
/*	template <>
	PrintLine operator << <Severity> (const Severity& s) { // Severity should always be the first value to operator <<
		return PrintLine("", m_ofs, m_edit, s >= m_sev_view, s >= m_sev_stream);
	}
*/
	static void setEditView(QTextEdit* edit) { instance().m_edit = edit; }

	static void setSeverity(Severity sev_stream, Severity sev_view) { instance().m_sev_stream = sev_stream; instance().m_sev_view = sev_view; } 

private:
    static Log& instance() { static Log log; return log; }

	std::ofstream m_ofs;

	QTextEdit* m_edit;

	Severity m_sev_stream;
	Severity m_sev_view;
};

template <>
Log::PrintLine Log::operator << <Log::Severity> (const Log::Severity& s) { // Severity should always be the first value to operator <<
    return PrintLine("", m_ofs, m_edit, s >= m_sev_view, s >= m_sev_stream);
}

inline Log& log() { return Log::instance(); }

namespace Ui {
    class LogView;
}

class LogView : public QDialog
{
    Q_OBJECT

public:
    explicit LogView(QWidget *parent = 0);
    ~LogView();

private:
    Ui::LogView *ui;

	QString m_str;
};

#endif // LOG_H
