#include <string.h>

#include <xdc/std.h>
#include <ti/xdais/xdas.h>

#include "read_params.h"

#include "log_to_file.h"

/*===========================================================================*/
/*!
*@func   ParameterNameToMapIndex
*
*@brief  Returns the index number from sTokenMap[] for a given parameter name.
*
*@param  s
*        parameter name string
*
*@return The index number if the string is a valid parameter name, -1 for error
*
*@note
*/
/*===========================================================================*/
XDAS_Int32 ParameterNameToMapIndex (xdc_Char *s, const sTokenMapping* sTokenMap)
{
    XDAS_Int32 i = 0;

    while (sTokenMap[i].tokenName != NULL)
    {
        if (0==strcmp (sTokenMap[i].tokenName, s))
            return i;
        else
            i++;
    }

    return -1;
}/* ParameterNameToMapIndex */


/*===========================================================================*/
/*!
*@func   ParseContent
*
*@brief  Parses the character array buf and writes global variable input.This is
*        necessary to facilitate the addition of new parameters through the
*        sTokenMap[] mechanism. Need compiler-generated addresses in sTokenMap.
*
*@param  buf
*        Pointer to the buffer to be parsed
*
*@param  bufsize
*        size of buffer
*
*@return XDAS_Int32
*        0 - Parsing Successful
*        -1 - Parsing Failure
*
*@note
*/
/*===========================================================================*/
XDAS_Int32 ParseContent(xdc_Char *buf, XDAS_Int32 bufsize, const sTokenMapping* sTokenMap)
{
    xdc_Char *items[CFG_MAX_ITEMS_TO_PARSE];
    XDAS_Int32 MapIdx;
    XDAS_Int32 item = 0;
    XDAS_Int32 InString = 0, InItem = 0;
    xdc_Char *p = buf;
    xdc_Char *bufend = &buf[bufsize];
    XDAS_Int32 IntContent;
    XDAS_Int32 i;
//    FILE *fpErr = stderr;

    /*------------------------------------------------------------------------*/
    /*                                STAGE ONE                               */
    /*------------------------------------------------------------------------*/
    /* Generate an argc/argv-type list in items[], without comments and
     * whitespace. This is context insensitive and could be done most easily
     * with lex(1).
     */
    while (p < bufend)
    {
        switch (*p)
        {
        case 13:
            p++;
            break;
        case '#': /* Found comment */
            *p = '\0'; /* Replace '#' with '\0' in case of comment */
            /* immediately following integer or string
             * Skip till EOL or EOF, whichever comes first
             */
            while (*p != '\n' && p < bufend)
            p++;
            InString = 0;
            InItem = 0;
            break;
        case '\n':
            InItem = 0;
            InString = 0;
            *p++='\0';
            break;
        case ' ':
        case '\t': /* Skip whitespace, leave state unchanged */
            if (InString)
                p++;
            else
            {   /* Terminate non-strings once whitespace is found */
                *p++ = '\0';
                InItem = 0;
            }
            break;
        case '"': /* Begin/End of String */
            *p++ = '\0';
            if (!InString)
            {
                items[item++] = p;
                InItem = ~InItem;
            }
            else
                InItem = 0;
                InString = ~InString; /* Toggle */
            break;
        default:
            if (!InItem)
            {
                items[item++] = p;
                InItem = ~InItem;
            }
            p++;
        }
    }

    item--;
    for (i=0; i<item; i+= 3)
    {
        if (0 > (MapIdx = ParameterNameToMapIndex (items[i], sTokenMap)))
        {
            log() << "Parameter Name " << std::string(items[i]) << " not recognized.";
            return -1;
        }

        if (strcmp ("=", items[i+1]))
        {
            log() << "file: '=' expected as the second token in each line.";
            return -1;
        }
        if(sTokenMap[MapIdx].bType == 1)
        {
            strcpy((xdc_Char *)sTokenMap[MapIdx].place, items[i+2]);
            /* sscanf (items[i+2], "%s", StringContent); */
        }
        else
        {
            sscanf (items[i+2], "%d", &IntContent);
            * ((XDAS_Int32 *) (sTokenMap[MapIdx].place)) = IntContent;
        }
    }

    return 0 ;
}/* ParseContent */


/*===========================================================================*/
/**
*@func   GetConfigFileContent
*
*@brief  Reads the configuration file content in a buffer and returns the
*        address of the buffer
*
*@param  fname
*        Pointer to the configuration file.
*
*@param  XDAS_Int8 *
*        Pointer to memory loaction holding configuration parameters
*
*@note
*/
/*===========================================================================*/
XDAS_Int32 GetConfigFileContent (FILE *fname, xdc_Char* buf)
{
    XDAS_Int32 FileSize;

    if (0 != fseek (fname, 0, SEEK_END))
    {
        return -1;
    }

    FileSize = ftell (fname);
    if (FileSize < 0 || FileSize > 20000)
    {
        return -1;
    }

    if (0 != fseek (fname, 0, SEEK_SET))
    {
        return -1;
    }

    /* Note that ftell() gives us the file size as the file system sees it.
     * The actual file size, as reported by fread() below will be often
     * smaller due to CR/LF to CR conversion and/or control characters after
     * the dos EOF marker in the file.
     */
    FileSize = fread (buf, 1, FileSize, fname);
    buf[FileSize] = '\0';
    fclose (fname);

    return 0;
}/* GetConfigFileContent */


/*===========================================================================*/
/*!
*@func   readparamfile
*
*@brief  Top Level function to read the parameter file.
*
*@param  fname
*        Pointer to the configuration file
*
*@return XDAS_Int32
*        0 - Param File Read Successful
*        -1 - Param File Read Failure
*
*@note
*/
/*===========================================================================*/
XDAS_Int32 readparamfile(const std::string& config, const sTokenMapping* tokenMap)
{
    xdc_Char *FileBuffer = new xdc_Char[CFG_FILE_MAX_SIZE];
    XDAS_Int32 retVal = 1;

    /* read the content in a buffer */
//    if(GetConfigFileContent(config, FileBuffer) >= 0)
	memcpy(FileBuffer, config.c_str(), std::min(config.size(), (size_t)CFG_FILE_MAX_SIZE));

	ParseContent(FileBuffer,strlen(FileBuffer), tokenMap);

    log() << "delete config file buffer";

    delete[] FileBuffer;
    log() << "config file buffer deleted";

    return retVal;
}/* readparamfile */
