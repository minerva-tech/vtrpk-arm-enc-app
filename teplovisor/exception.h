/*
 * exception.h
 *
 *  Created on: Jan 8, 2013
 *      Author: a
 */

#include <string>

enum ex_flag_t {
    Ex_NoFlag = 0,
    Ex_TryAnotherConfig
};

class ex : public std::exception
{
public :
	ex(const std::string& str, ex_flag_t f=Ex_NoFlag) : m_str(str), m_flag(f) { }
	~ex() throw() { }
	const std::string& str() const { return m_str; }
    const ex_flag_t flag() const { return m_flag; }

private:
	const std::string m_str;
    const ex_flag_t m_flag;
};
