/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_MYMONITOR_H
#define PHP_MYMONITOR_H

extern zend_module_entry mymonitor_module_entry;
#define phpext_mymonitor_ptr &mymonitor_module_entry

#ifdef PHP_WIN32
#	define PHP_MYMONITOR_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_MYMONITOR_API __attribute__ ((visibility("default")))
#else
#	define PHP_MYMONITOR_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define MINIT_SHM_KEY 999
#define FROM_MINIT "from_minit"
#define WRITE_LOG(format,...)  do {                         \
            char strTime[20];                                  \
            getST(strTime);                                 \
            FILE *fp = NULL;                                \
            fp = fopen("/data1/www/applogs/monitor","a+");  \
            fprintf(fp,"[%s] ", strTime);                   \
            fprintf(fp, format, __VA_ARGS__);               \
            fclose(fp);                                     \
    }while(0)                                               

PHP_MINIT_FUNCTION(mymonitor);
PHP_MSHUTDOWN_FUNCTION(mymonitor);
PHP_RINIT_FUNCTION(mymonitor);
PHP_RSHUTDOWN_FUNCTION(mymonitor);
PHP_MINFO_FUNCTION(mymonitor);

PHP_FUNCTION(confirm_mymonitor_compiled);	/* For testing, remove later. */
PHP_FUNCTION(get_req_status);
PHP_FUNCTION(get_system_info);

typedef struct _http_code_dstat {
        char *code;
        char *str;
        long num;
} http_code_dstat;

typedef struct _shm_request_status{
    char *stat_id;
    int starttime;
    http_code_dstat *code_arr;
} shm_req_status;

struct shm_struct
{
        int shmid;
        int key;
        int shmflg;
        int shmatflg;
        shm_req_status *addr;
        int size;
};

typedef struct _request_status
{
    char    *stat_id;
    int     starttime;
    http_code_dstat *code_arr;
    struct shm_struct *shm_p;
} req_status;


typedef struct _cpu_data
{
    char name[20];
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;
    unsigned int iowait;
    unsigned int irq;
    unsigned int softirq;
} cpu_data;

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     
*/

ZEND_BEGIN_MODULE_GLOBALS(mymonitor)    
	long start_on_minit;
    req_status *request_status;
ZEND_END_MODULE_GLOBALS(mymonitor)


/* In every utility function you add that needs to use variables 
   in php_mymonitor_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as MYMONITOR_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define MYMONITOR_G(v) TSRMG(mymonitor_globals_id, zend_mymonitor_globals *, v)
#else
#define MYMONITOR_G(v) (mymonitor_globals.v)
#endif

#endif	/* PHP_MYMONITOR_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
