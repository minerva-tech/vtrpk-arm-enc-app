/*
 * exception.h
 *
 *  Created on: Jan 8, 2013
 *      Author: a
 */

#include <string>

class ex : public std::exception
{
public :
	ex(const std::string& str) : m_str(str) { }
	~ex() throw() { }
	std::string& str() { return m_str; }

private:
	std::string m_str;
};
