/*-------------------------------------------------------------------------*/
/**
   @file    tcgi.h
   @author  N. Devillard
   @date    Oct 2008
   @version	$Revision: 1.2 $
   @brief   Tiny CGI library
*/
/*--------------------------------------------------------------------------*/
/*
	$Id: tcgi.h,v 1.2 2008/10/23 13:58:23 nicoldev Exp $
	$Date: 2008/10/23 13:58:23 $
*/

/*---------------------------------------------------------------------------
   								Includes
 ---------------------------------------------------------------------------*/
#include "dict.h"

/*---------------------------------------------------------------------------
   							Function prototypes
 ---------------------------------------------------------------------------*/

/*
 * Determine if the current program is being called in a CGI context
 */
int tcgi_active(void);

/**
 * Parse a CGI request and return results into an easily queried data
 * structure.
 * This function discovers all CGI parameters located in environment
 * variables and coming from stdin, decodes URL encoding and stores results
 * into a dictionary.
 * The following variables can be queried:
    - SERVER_SOFTWARE
    - SERVER_NAME
    - GATEWAY_INTERFACE
    - SERVER_PROTOCOL
    - SERVER_PORT
    - REQUEST_METHOD
    - PATH_INFO
    - PATH_TRANSLATED
    - SCRIPT_NAME
    - QUERY_STRING
    - REMOTE_HOST
    - REMOTE_ADDR
    - AUTH_TYPE
    - REMOTE_USER
    - REMOTE_IDENT
    - CONTENT_TYPE
    - CONTENT_LENGTH
    - HTTP_ACCEPT
    - HTTP_USER_AGENT
    - HTTP_COOKIES
    
  * In addition, variables set as a result of a GET or POST request are
  * stored alongside these variables. For example, if the program was
  * called as a result from a URL like:
  *
  * /cgi/program.cgi?alpha=1&beta=2
  *
  * The variables "alpha" and "beta" will be stored in the
  * dictionary with values "1" and "2".
  *
  * Cookies are also parsed and placed into the structure with the
  * following format:
  *
  */
dictionary * tcgi_parse(void);

/* vim: set ts=4 et sw=4 tw=75 */
