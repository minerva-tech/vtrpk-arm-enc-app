#ifndef LOG_H
#define LOG_H

#pragma warning(disable : 4995)

#include <boost/date_time.hpp>

class Log {
	friend Log& log();
public:
    Log() : /*m_ofs("./log.txt", std::ios_base::app),*/ m_sev_stream(Debug)
	{ /*m_ofs << "[" << boost::gregorian::day_clock::local_day() << "]" << std::endl;*/ }

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
		template <typename T>  // BTW, for char[] input it generates different methods for different string len. I.e. log() << "hello" and log() << "hi" are TWO different functions generated. Looks like crap.
		PrintLine& operator << (const T& v) {
			if (m_write_to_stream){
//				m_ofs << v;
				std::cout << v;
			}
			return *this;
		}

//		template <>
        PrintLine& operator << /*<std::string>*/(const std::string& v) { // qt shit
			if (m_write_to_stream) {
//				m_ofs << v;
				std::cout << v;
			}
			return *this;
		}

		~PrintLine() { 
			if (m_write_to_stream) {
//				m_ofs << std::endl << std::flush;
				std::cout << std::endl << std::flush;
			}
		}

	private:
		template <typename T>
		PrintLine(const T& v, /*std::ofstream& ofs,*/ bool write_to_stream) : 
//			m_ofs(ofs), 
			m_write_to_stream(write_to_stream)
		{
			std::ostringstream time;
			time << "[" << boost::posix_time::second_clock::local_time().time_of_day() << "] ";
			*this << time.str();
			*this << v;
		}

//		std::ofstream& m_ofs;

		const bool m_write_to_stream;
	};

	template <typename T>
	PrintLine operator<< (const T& v) {
		return PrintLine(v, /*m_ofs,*/ Warning >= m_sev_stream);
	}

//	template <> // when it is uncommented, g++ cannot build this class.
//	PrintLine operator << <Severity> (const Severity& s);
/*	template <>
	PrintLine operator << <Severity> (const Severity& s) { // Severity should always be the first value to operator <<
		return PrintLine("", m_ofs, m_edit, s >= m_sev_view, s >= m_sev_stream);
	}
*/
	static void setSeverity(Severity sev_stream) { instance().m_sev_stream = sev_stream; } 

private:
    static Log& instance() { static Log log; return log; }

//	std::ofstream m_ofs;

	Severity m_sev_stream;
};

template <>
inline Log::PrintLine Log::operator << <Log::Severity> (const Log::Severity& s) { // Severity should always be the first value to operator <<
    return PrintLine("", /*m_ofs,*/ s >= m_sev_stream);
}

inline Log& log() { return Log::instance(); }

#endif // LOG_H
