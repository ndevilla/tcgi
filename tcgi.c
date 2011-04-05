/*-------------------------------------------------------------------------*/
/**
   @file    tcgi.c
   @author  N. Devillard
   @date    Apr 2011
   @brief   CGI engine

   This single source file implements a parser for CGI requests coming
   from a web server. It supports GET and POST and will correctly parse
   POST form data, both URL-encoded and multipart.
*/
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
   								Includes
 ---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <regex.h>

#include "dict.h"

/*---------------------------------------------------------------------------
   								Defines
 ---------------------------------------------------------------------------*/
/** Max accepted size for an URL */
#define URL_MAX_SZ          1024
/** Empty value in a dictionary */
#define EMPTY_VALUE         "empty"
/** Boundary signal in form-encoded data */
#define ENCODED_BOUNDARY    "boundary="

/** Regexp for Content-Disposition */
#define C_DISP "[Cc]ontent-[Dd]isposition: form-data;[^\n]*name=\"([^\"]*)\""
/** Regexp for Content-Type */
#define C_TYPE "[Cc]ontent-[Tt]ype: ([^\n]+)"
/** Regexp for Content-Transfer-Encoding */
#define C_CODE "[Cc]ontent-[Tt]ransfer-[Ee]ncoding: ([^\n]+)"
/** Regexp for Content */
#define C_CONT "\n\n(.*)\n$"


/*---------------------------------------------------------------------------
   								Macros
 ---------------------------------------------------------------------------*/
/** Debug mode only */
#define DEBUG_MODE  0
#if DEBUG_MODE
#define debug_code(x)   x
#else
#define debug_code(x)
#endif

/*---------------------------------------------------------------------------
  							Function codes
 ---------------------------------------------------------------------------*/

/* stolen from NCSA code */
static char x2c(char *what)
{
    char digit;

    digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A')+10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A')+10 : (what[1] - '0'));
    return digit ;
}

/* stolen from NCSA code */
static void unescape_url(char *url)
{
    int x,y;

    for (x=0, y=0 ; url[y] ; ++x, ++y) {
        if((url[x] = url[y]) == '%') {
            url[x] = x2c(&url[y+1]);
            y+=2;
        }
      }
    url[x] = '\0';
}

/* Replace a char by another in a writable string */
static void replace(char * s, char prev, char next)
{
    int i ;

    if (!s)
        return ;

    for (i=0 ; s[i] ; i++) {
        if (s[i]==prev) {
            s[i]=next ;
        }
    }
    return ;
}

/* Parse a URL and put detected variables into dictionay */
static int tcgi_parse_url(dict * cgid, char * url)
{
    char * url_cp ;
    int    sz ;
    char * decl ;
    char * ctx=0 ;
    char   key[URL_MAX_SZ+1] ;
    char   val[URL_MAX_SZ+1] ;

    if (cgid==NULL || url==NULL) {
        return -1 ;
    }
    sz = (int)strlen(url);
    if (sz<1 || sz>=URL_MAX_SZ) {
        return -1 ;
    }

    url_cp = calloc(sz+1, 1);
    if (url_cp==NULL) {
        return -1 ;
    }
    memcpy(url_cp, url, sz);
    debug_code(
        printf(" url-buffer: [%s]\n", url_cp);
    );

    /* Cut input into tokens */
    decl = strtok_r(url_cp, "&", &ctx);
    if (decl) {
        do {
            memset(key, 0, URL_MAX_SZ+1);
            memset(val, 0, URL_MAX_SZ+1);
            sscanf(decl, "%[^=]=%s", key, val);
            unescape_url(val);
            replace(val, '+', ' ');
            debug_code(
                printf("url-decoded:\n\tkey: [%s]\n\tval: [%s]\n", key, val);
            );
            dict_add(cgid, key, val);
            decl = strtok_r(NULL, "&", &ctx);
        } while (decl);
    }
    free(url_cp);
    return 0 ;
}

/* Parse a POST block and put detected variables into dictionary */
static int tcgi_blockprocess(dict * cgid, char * block, int blocksz)
{
    regex_t     c_disp,
                c_cont ;
    regmatch_t  pmatch[2] ;

    int     len ;
    int     err ;
    char *  key=0 ;
    char *  val=0 ;

    if (!cgid || !block || blocksz<1) {
        return -1 ;
    }
    regcomp(&c_disp, C_DISP, REG_EXTENDED);
    regcomp(&c_cont, C_CONT, REG_EXTENDED);

    err = regexec(&c_disp, block, 2, pmatch, 0);
    if (!err) {
        len=pmatch[1].rm_eo - pmatch[1].rm_so ;
        if (len>0) {
            key=calloc(len+1, 1);
            if (key==NULL) {
                regfree(&c_disp);
                return -1 ;
            }
            strncpy(key, block+pmatch[1].rm_so, len);
        }
    }
    err = regexec(&c_cont, block, 2, pmatch, 0);
    if (!err) {
        len=pmatch[1].rm_eo - pmatch[1].rm_so ;
        if (len>0) {
            val=calloc(len+1, 1);
            if (val==NULL) {
                free(key);
                regfree(&c_disp);
                regfree(&c_cont);
                return -1 ;
            }
            strncpy(val, block+pmatch[1].rm_so, len);
        }
    }
    if (key && val) {
        dict_add(cgid, key, val);
        debug_code(
            printf("frm-decoded:\n\tkey: [%s]\n\tval: [%s]\n", key, val);
        );
    }
    if (key) free(key) ;
    if (val) free(val) ;

    regfree(&c_disp);
    regfree(&c_cont);
    return 0 ;
}

/* Parse POST form data and put detected variables into dictionary */
static int tcgi_parse_formdata(dict * cgid, char * buffer, int sz)
{
    char * val =0;
    char * boundary ;
    int    boundlen ;
    char * content_type =0;

    char * beg =0;
    char * end =0;
    char * block =0;
    int    blocksz=0 ;
    int    i ;

    /* Retrieve boundary into a string */
    content_type = dict_get(cgid, "CONTENT_TYPE", EMPTY_VALUE);
    if (!content_type || !strcmp(content_type, EMPTY_VALUE)) {
        return -1 ;
    }
    val = strstr(content_type, ENCODED_BOUNDARY);
    if (!val) {
        return -1 ;
    }
    val+=strlen(ENCODED_BOUNDARY);
    boundlen=strlen(val)+3 ; /* 3 chars: prefix '--' and trailing zero */
    boundary=malloc(boundlen);
    if (boundary==NULL) {
        return -1 ;
    }
    boundary[0]='-';
    boundary[1]='-';
    strcpy(boundary+2, val);

    /* Find first boundary-limited block */
    beg = strstr(buffer, boundary);
    if (!beg) {
        free(boundary);
        return 0;
    }
    /* Parse each block */
    while (beg) {
        beg+=boundlen ;
        end = strstr(beg, boundary);
        if (end) {
            blocksz = end-beg ;
            block=malloc(blocksz+1);
            if (block==NULL) {
                free(boundary);
                return -1 ;
            }
            for (i=0; i<blocksz ; i++) {
                block[i]=beg[i];
            }
            block[blocksz]=(char)0 ;
            /* Process individual block */
            tcgi_blockprocess(cgid, block, blocksz);
            free(block);
        }
        beg=end ;
    }
    free(boundary);
    /* Dictionary has been updated for each block */
    return 0 ;
}

/* Parse a GET request into dictionary */
static int tcgi_parse_GET(dict * cgid)
{
    char * query ;

    if (cgid==NULL) {
        return -1 ;
    }
    /* Get GET parameters */
    query = dict_get(cgid, "QUERY_STRING", NULL);
    if (!query || !strcmp(query, EMPTY_VALUE)) {
        return 0 ;
    }
    return tcgi_parse_url(cgid, query) ;
}

/* Strip a writable character string of an unwanted character */
static void strip(char * buffer, char unwanted)
{
    int i, j ;
    int len ;

    if (!buffer)
        return ;

    len = (int)strlen(buffer);

    for (i=0, j=0 ; j<len ; j++) {
        if (buffer[j]!=unwanted) {
            buffer[i]=buffer[j] ;
            i++ ;
        }
    }
    buffer[i]=0 ; 
}

/* Parse a POST request into a dictionary */
static int tcgi_parse_POST(dict * cgid)
{
    int     content_len ;
    char *  content_type ;
    char *  val ;
    char *  buffer ;

    if (cgid==NULL) {
        return -1 ;
    }

    content_type = dict_get(cgid, "CONTENT_TYPE", EMPTY_VALUE);
    if (!content_type || !strcmp(content_type, EMPTY_VALUE)) {
        return -1 ;
    }
    val = dict_get(cgid, "CONTENT_LENGTH", EMPTY_VALUE);
    if (!val || !strcmp(val, EMPTY_VALUE)) {
        return -1 ;
    }
    content_len = atoi(val);
    buffer = calloc(content_len + 1, sizeof(char));
    if (!buffer) {
        return -1 ;
    }
    if (fread(buffer, sizeof(char), content_len, stdin) != content_len) {
        free(buffer);
        return -1 ;
    }
    strip(buffer, '\r');

    /* Extract data from buffer */
    if (!strncmp(content_type, "application/x-www-form-urlencoded", 33)) {
        tcgi_parse_url(cgid, buffer);
    } else if (!strncmp(content_type, "multipart/form-data", 19)) {
        tcgi_parse_formdata(cgid, buffer, content_len);
    } else {
        dict_add(cgid, "content", buffer);
    }
    free(buffer);
    return 0 ;
}

/*
 * Determine if the current program is being called in a CGI context
 */
int tcgi_active(void)
{
    int active=0 ;
    
    if (getenv("NOCGI"))
        return 0 ;

    if (getenv("SERVER_SOFTWARE") &&
        getenv("SERVER_NAME") &&
        getenv("GATEWAY_INTERFACE"))
        active=1 ;
    return active ;
}

/* Parse a CGI request and place results into a dictionary */
dict * tcgi_parse(void)
{
    dict     * cgid ;
    char     * val ;
    int        i ;
    char     * env_vars[] = {
        "SERVER_SOFTWARE",
        "SERVER_NAME",
        "GATEWAY_INTERFACE",
        "SERVER_PROTOCOL",
        "SERVER_PORT",
        "REQUEST_METHOD",
        "PATH_INFO",
        "PATH_TRANSLATED",
        "SCRIPT_NAME",
        "QUERY_STRING",
        "REMOTE_HOST",
        "REMOTE_ADDR",
        "AUTH_TYPE",
        "REMOTE_USER",
        "REMOTE_IDENT",
        "CONTENT_TYPE",
        "CONTENT_LENGTH",
        "HTTP_ACCEPT",
        "HTTP_USER_AGENT",
        "HTTP_COOKIES",
        NULL
    };

    cgid = dict_new();

    /* Read in all environment */
    for (i=0 ; env_vars[i]!=NULL ; i++) {
        val = getenv(env_vars[i]);
        dict_add(cgid, env_vars[i], val ? val : EMPTY_VALUE);
    }

    /* Parse GET parameters, i.e. the URL found in QUERY_STRING */
    tcgi_parse_GET(cgid);
    /* Parse POST parameters: stuff coming from stdin */
    tcgi_parse_POST(cgid);

    return cgid ;
}

#ifdef TCGI_MAIN
int main(int argc, char * argv[])
{
    dict * cgid;

    printf("Content-type: text/plain\r\n\r\n");
    cgid = tcgi_parse();
    dict_dump(cgid, stdout);
    dict_free(cgid);
	return 0 ;
}
#endif

/* vim: set ts=4 et sw=4 tw=75 */
