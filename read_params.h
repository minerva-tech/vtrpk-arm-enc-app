/*
 * read_params.h
 *
 *  Created on: Jan 20, 2013
 *      Author: a
 */

#ifndef READ_PARAMS_H_
#define READ_PARAMS_H_

#include <stdio.h>

const int CFG_MAX_ITEMS_TO_PARSE = 1000;
const int CFG_FILE_MAX_SIZE = 20000;

struct sTokenMapping
{
    xdc_Char *tokenName;
    xdc_Char bType;
    XDAS_Void *place;
};

//XDAS_Int32 ParseContent (xdc_Char *buf, XDAS_Int32 bufsize);
XDAS_Int32 readparamfile(FILE * fname, const sTokenMapping* tokenMap);

#endif /* READ_PARAMS_H_ */
