#ifndef ERRORS_H
#define ERRORS_H

#include "logview.h"
#include <string>

class exception : public std::exception
{
public:
	exception(const std::string& desc, const char* file, int line) : m_desc(desc), m_file(file), m_line(line) {
		log() << text();
	}

    virtual ~exception() throw() { }
#if _WIN32
	std::string text() {return std::string("Exception is thrown, description: ") + m_desc + ", file: " + m_file + ", line: " + std::to_string(m_line);}
#else
    std::string text() {return std::string("Exception is thrown, description: ") + m_desc + ", file: " + m_file + ", line: ";}
#endif

private:
	std::string m_desc;
	std::string m_file;
	long long	m_line;
};

#define THR(a, text) if (!a) throw exception(text, __FILE__, __LINE__);

//! Used for optional DS stuff. If call is not succeeded, log is written, and excecution is continued. But no DS view is available, of course.
#define DS_OPT(a) if (!SUCCEEDED(a)) { log() << #a << " was failed in file: " << __FILE__ ", line: " << __LINE__; }

#endif
