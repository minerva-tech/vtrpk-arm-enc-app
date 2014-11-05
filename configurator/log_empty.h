#ifndef LOG_EMPTY_H
#define LOG_EMPTY_H

class Log {
public:
	enum Severity {
		Dump = 0,
		Debug,
		Warning,
		Error,
		Critical
	};

	class PrintLine {
	public:
		template <typename T>
		PrintLine& operator<< (const T& v) { return *this; }
	};

	template <typename T>
	PrintLine operator<< (const T& v) { return PrintLine(); }
};

inline Log log() { return Log(); }

#endif // LOG_EMPTY_H
