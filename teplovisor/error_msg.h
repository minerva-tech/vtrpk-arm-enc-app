/*
 * error_msg.h
 *
 *  Created on: Jan 22, 2013
 *      Author: a
 */

#ifndef ERROR_MSG_H_
#define ERROR_MSG_H_

#include <xdc/std.h>
#include <ti/xdais/xdas.h>
#include "h264venc.h"
#include <string>

std::string printErrorMsg(XDAS_Int32 errorCode);

#endif /* ERROR_MSG_H_ */
