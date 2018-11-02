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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_mymonitor.h"

#include "SAPI.h"
#include "ext/standard/php_var.h"
#include <sys/sysinfo.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>

#define IF_MYMONITOR_VARS() \
        if (MYMONITOR_G(request_status) && MYMONITOR_G(request_status)[0] != NULL)

/* If you declare any globals in php_mymonitor.h uncomment this:*/
ZEND_DECLARE_MODULE_GLOBALS(mymonitor)

/* True global resources - no need for thread safety here */
static int le_mymonitor;
static int le_shm;

/* {{{ mymonitor_functions[]
 *
 * Every user visible function must have an entry in mymonitor_functions[].
 */
const zend_function_entry mymonitor_functions[] = {
	PHP_FE(confirm_mymonitor_compiled,	NULL)		/* For testing, remove later. */ 
    PHP_FE(get_req_status,  NULL)
    PHP_FE(get_system_info, NULL)
	PHP_FE_END	/* Must be the last line in mymonitor_functions[] */
};
/* }}} */

/* {{{ mymonitor_module_entry
 */
zend_module_entry mymonitor_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"mymonitor",
	mymonitor_functions,
	PHP_MINIT(mymonitor),
	PHP_MSHUTDOWN(mymonitor),
	PHP_RINIT(mymonitor),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(mymonitor),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(mymonitor),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MYMONITOR
ZEND_GET_MODULE(mymonitor)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("mymonitor.start_on_phpstart",      "1", PHP_INI_SYSTEM, OnUpdateBool, start_on_minit, zend_mymonitor_globals, mymonitor_globals)
PHP_INI_END()
/* }}} */

static int getTS(){
    time_t t;
    return time(&t);
}

static void getST(char *param){
    time_t t;
    time(&t);
    struct tm *_tm = localtime(&t);
    strftime (param, 20, "%Y/%m/%d %H:%M:%S", _tm);
}

/* {{{ php_mymonitor_init_globals
 */
static void php_mymonitor_init_globals(zend_mymonitor_globals *mymonitor_globals)
{
	mymonitor_globals->start_on_minit = 1;
}
/* }}} */

static void clean_monitor_res_persist(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    req_status *req_status_data = (req_status*)rsrc->ptr;
    
    shmdt(req_status_data->shm_p->addr);
    pefree(req_status_data->shm_p,1);
    pefree(req_status_data->stat_id,1);
    pefree(req_status_data->code_arr,1);
}
/* }}} */
/*
static void clean_monitor_res(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    req_status *req_status_data = (req_status *)rsrc->ptr;

    shmdt(req_status_data->shm_p->addr);
    pefree(req_status_data->shm_p,1);
    pefree(req_status_data->stat_id,1);
    pefree(req_status_data->code_arr,1);
}*/

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(mymonitor)
{
	REGISTER_INI_ENTRIES();
    //global结构体的构造函数和析构函数
    ZEND_INIT_MODULE_GLOBALS(mymonitor, php_mymonitor_init_globals, NULL);
    //注册一个常量FROM_MINIT
    REGISTER_STRINGL_CONSTANT("FROM_MINIT",     FROM_MINIT, sizeof(FROM_MINIT)-1, CONST_PERSISTENT | CONST_CS);
    //将共享内存key注册为常量
    REGISTER_LONG_CONSTANT("MINIT_SHM_KEY",     MINIT_SHM_KEY,  CONST_PERSISTENT | CONST_CS);
       
    //注册资源类型
    //EG列表删除回调NULL，持久化列表删除clean_monitor_res_persist
    le_mymonitor = zend_register_list_destructors_ex(NULL, clean_monitor_res_persist, "monitor_res", module_number);
    //判断有没有开启默认统计，开启了则注册一个持久化资源
    if(MYMONITOR_G(start_on_minit))
    { 
        req_status *req_status_data;
        req_status_data = (req_status*)pemalloc(sizeof(req_status),1);
        req_status_data->starttime = getTS();
        req_status_data->stat_id = pestrdup(FROM_MINIT,1);
        req_status_data->code_arr = (http_code_dstat*)pemalloc(sizeof(http_code_dstat)*50,1);

        http_code_dstat tmp_code_arr[] = 
        {
            {"100", "Continue", 0},
            {"101", "Switching Protocols", 0},
            {"200", "OK", 0},
            {"201", "Created", 0},
            {"202", "Accepted", 0},
            {"203", "Non-Authoritative Information", 0},
            {"204", "No Content", 0},
            {"205", "Reset Content", 0},
            {"206", "Partial Content", 0},
            {"300", "Multiple Choices", 0},
            {"301", "Moved Permanently", 0},
            {"302", "Moved Temporarily", 0},
            {"303", "See Other", 0},
            {"304", "Not Modified", 0},
            {"305", "Use Proxy", 0},
            {"400", "Bad Request", 0},
            {"401", "Unauthorized", 0},
            {"402", "Payment Required", 0},
            {"403", "Forbidden", 0},
            {"404", "Not Found", 0},
            {"405", "Method Not Allowed", 0},
            {"406", "Not Acceptable", 0},
            {"407", "Proxy Authentication Required", 0},
            {"408", "Request Time-out", 0},
            {"409", "Conflict", 0},
            {"410", "Gone", 0},
            {"411", "Length Required", 0},
            {"412", "Precondition Failed", 0},
            {"413", "Request Entity Too Large", 0},
            {"414", "Request-URI Too Large", 0},
            {"415", "Unsupported Media Type", 0},
            {"500", "Internal Server Error", 0},
            {"501", "Not Implemented", 0},
            {"502", "Bad Gateway", 0},
            {"503", "Service Unavailable", 0},
            {"504", "Gateway Time-out", 0},
            {"505", "HTTP Version not supported", 0},
            {NULL,   NULL, 0}
        };
        memcpy(req_status_data->code_arr,tmp_code_arr,sizeof(http_code_dstat)*50);
        MYMONITOR_G(request_status) = (req_status*)pemalloc(sizeof(req_status),1);
        MYMONITOR_G(request_status)->starttime = 10;
        
        //获取是否已创建的共享内存
        int shmid;
        int is_create = 0;
        shmid = shmget((key_t)MINIT_SHM_KEY, 0, 0); 
        if (shmid == -1)
        {
            //创建(或获取)共享内存资源 
            shmid = shmget((key_t)MINIT_SHM_KEY, 3000, IPC_CREAT | 0666);
            if(shmid == -1){
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to attach or create shared memory segment");
                pefree(req_status_data->code_arr,1);
                pefree(req_status_data,1);
                return SUCCESS; 
            }
            is_create = 1;
        }
        
        //控制共享内存
        struct shmid_ds shm;
        if (shmctl(shmid, IPC_STAT, &shm)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to get shared memory segment information");
            pefree(req_status_data->code_arr,1);
            pefree(req_status_data,1);
            return SUCCESS; 
        }
        //将共享内存关联到当前进程
        shm_req_status *p;
        p = (shm_req_status*)shmat(shmid, NULL, IPC_CREAT);
        WRITE_LOG("shm 地址 [%p]\n", p);
        if (p == (void*) -1) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "unable to attach to shared memory segment");
            pefree(req_status_data->code_arr,1);
            pefree(req_status_data,1);
            return SUCCESS; 
        }

        //共享内存初始化数据
        if(is_create == 1){
            WRITE_LOG("p address [%p] \n",p);
            
            memset(p,0,3000);
            p->stat_id = (char*)(p+ sizeof(shm_req_status));
            memcpy(p->stat_id,FROM_MINIT,sizeof(FROM_MINIT));
            p->code_arr = (http_code_dstat*)(p+ sizeof(shm_req_status) + sizeof(FROM_MINIT));
            p->starttime = getTS();
            
            http_code_dstat *tmp_p = p->code_arr;
            int i =0;
            while(i <= 40){
                if(tmp_code_arr[i].code == NULL){
                    tmp_p->code = NULL;
                    tmp_p->str = NULL;
                    tmp_p->num = 0;
                    break;
                }
                if(i == 0){
                    tmp_p->code = (char*)(tmp_p + 40);
                }else{
                    tmp_p->code = (char*)((tmp_p-1)->str + strlen((tmp_p-1)->str)+1);
                }
                tmp_p->str = (char*)(tmp_p->code + strlen(tmp_code_arr[i].code) +1);
                strcpy(tmp_p->code,tmp_code_arr[i].code);
                strcpy(tmp_p->str,tmp_code_arr[i].str);
                tmp_p->num = 0;
                
                i++;
                tmp_p++;
            }
            
            WRITE_LOG("str last addr[%p]\n",((shm_req_status*)p)->code_arr[36].str);
        }else{
            p->stat_id = (char*)(p+ sizeof(shm_req_status));
            p->code_arr = (http_code_dstat*)(p+ sizeof(shm_req_status) + sizeof(FROM_MINIT));
            http_code_dstat *tmp_p = p->code_arr;
            WRITE_LOG("p tmp_p end[%p]\n",tmp_p->code);
            int i =0;
            while(i <= 40){
                if(tmp_code_arr[i].code == NULL){
                    tmp_p->code = NULL;
                    tmp_p->str = NULL;
                    break;
                }
                if(i == 0){
                    tmp_p->code = (char*)(tmp_p + 40);
                }else{
                    tmp_p->code = (char*)((tmp_p-1)->str + strlen((tmp_p-1)->str)+1);
                }
                tmp_p->str = (char*)(tmp_p->code + strlen(tmp_code_arr[i].code) +1);
                i++;
                tmp_p++;
            }
            
            WRITE_LOG("first code[%s]\n",((shm_req_status*)p)->code_arr[0].code);
        }
        
        req_status_data->shm_p = (struct shm_struct*)pemalloc(sizeof(struct shm_struct), 1);
        struct shm_struct *shm_p;
        shm_p = (struct shm_struct*)emalloc(sizeof(struct shm_struct));
        shm_p->shmid = shmid;
        shm_p->key = MINIT_SHM_KEY;
        shm_p->shmflg = IPC_CREAT;
        shm_p->shmatflg = IPC_CREAT;
        shm_p->addr = p;
        shm_p->size = shm.shm_segsz;
        memcpy(req_status_data->shm_p, shm_p, sizeof(struct shm_struct));

        //将资源存储到EG(regular_list)列表，返回资源ID
        int rsid = zend_list_insert(req_status_data, le_mymonitor TSRMLS_CC);
        //int rsid2 = zend_list_insert(shm_p, le_shm TSRMLS_CC);
        
        //将资源存储到持久化列表EG(persist_list)
        zend_rsrc_list_entry *le;
        le = (zend_rsrc_list_entry *) pemalloc(sizeof(zend_rsrc_list_entry),1);
        le->type = le_mymonitor;
        le->ptr = req_status_data;//testZval;

        char *hash_key;
        int hash_key_len = spprintf(&hash_key,0,"%s",FROM_MINIT);
        zend_hash_update(&EG(persistent_list), hash_key, hash_key_len + 1, (void*)le, sizeof(zend_rsrc_list_entry), NULL);
        efree(hash_key);

        //将共享内存作为资源，持久化
        /*le_shm = zend_register_list_destructors_ex(NULL, clean_monitor_res, "monitor_res_shm", module_number); 
        int rsid_shm = zend_list_insert(shm_p, le_shm TSRMLS_CC);
 
        zend_rsrc_list_entry *le_s;
        le_s = (zend_rsrc_list_entry *) pemalloc(sizeof(zend_rsrc_list_entry),1);
        le_s->type = le_shm;
        le_s->ptr = shm_p;

        char *key_shm;
        int key_shm_len = spprintf(&hash_key,0,"php_req_status_shm_%s",FROM_MINIT);
        zend_hash_update(&EG(persistent_list), key_shm, key_shm_len + 1, (void*)le_s, sizeof(zend_rsrc_list_entry), NULL);
        efree(key_shm);
*/
    }

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mymonitor)
{
	UNREGISTER_INI_ENTRIES();
    pefree(MYMONITOR_G(request_status),1);
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(mymonitor)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(mymonitor)
{

    zend_rsrc_list_entry *le; 
    char *hash_key;
    int hash_key_len = spprintf(&hash_key,0,"%s",FROM_MINIT);
    
    //http_code 获取
    char *code_response;
    int code_len = spprintf(&code_response,0,"%ld",SG(sapi_headers).http_response_code);
    //获取持久化资源
    int res = zend_hash_find(&EG(persistent_list), hash_key, hash_key_len + 1, (void**)&le);
    if(res == SUCCESS)
    {
        //成功拿出来
        WRITE_LOG("[%d]:资源类型 [%d], 进程号 [%d] \n",__LINE__, le->type, getpid());
        //循环找到http_code对应内存并加1
        shm_req_status *tmp_req_status = ((req_status*)le->ptr)->shm_p->addr;
        http_code_dstat *code_arr = tmp_req_status->code_arr;
        
        while(code_arr->code != NULL)
        {
            if(strcmp(code_arr->code, code_response) == 0){
                __sync_fetch_and_add(&code_arr->num, 1);
                WRITE_LOG("[%d]:增加的code [%s],num [%d]\n",__LINE__, code_arr->code, code_arr->num);
                break;
            }
            code_arr++;
        }
        WRITE_LOG("[%d]: 共享内存数据 stime_stat_id [%d]_[%s],改变的 code:[%s],num:[%ld] \n", __LINE__, tmp_req_status->starttime, tmp_req_status->stat_id, code_arr->code,code_arr->num);
        
        http_code_dstat *code_arr_persis = ((req_status*)le->ptr)->code_arr;
        while(code_arr_persis->code != NULL)
        {
            if(strcmp(code_arr_persis->code, code_response) == 0){
                __sync_fetch_and_add(&code_arr_persis->num, 1);
                break;
            }
            code_arr_persis++;
        }
        WRITE_LOG("[%d]: 持久化数据 stime[%d],改变的 code:[%s],num:[%ld] \n", __LINE__, ((req_status*)le->ptr)->starttime, code_arr_persis->code, code_arr_persis->num);
        efree(code_response);
    }
    else
    {
        WRITE_LOG("[%d]:RINIT 中获取持久化资源失败\n",__LINE__);    
    }
    efree(hash_key);
	
    return SUCCESS;
}
/* }}} */

/*获取cpu和mem信息
 * {{{ get_system_info
 */
PHP_FUNCTION(get_system_info)
{
	zend_bool is_cpu,is_mem;
	zval *ret;
    char *hostname;
    extern int errno;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b|b", &is_cpu, &is_mem) == FAILURE) {
		return;
	}

	//给返回数组分配内存初始化操作
    MAKE_STD_ZVAL(ret);
	array_init(ret);
    
    hostname = emalloc(512);
    if(gethostname(hostname, sizeof(hostname)) == SUCCESS){
        add_assoc_string(ret, "hostname", hostname, 1);   
    }else{
        add_assoc_string(ret, "hostname", strerror(errno), 1);
    }
   
    if(is_mem){
        zval *mem_zval;
        MAKE_STD_ZVAL(mem_zval);
        array_init(mem_zval);

        //获取系统内存
        struct sysinfo sys_info;
        if(sysinfo(&sys_info) == 0){
            add_assoc_long(mem_zval, "totalram", sys_info.totalram/1024);
            add_assoc_long(mem_zval, "freeram", sys_info.freeram/1024);
            add_assoc_long(mem_zval, "sharedram", sys_info.sharedram/1024);
            add_assoc_long(mem_zval, "bufferram", sys_info.bufferram/1024);
            add_assoc_long(mem_zval, "totalswap", sys_info.totalswap/1024);
            add_assoc_long(mem_zval, "freeswap", sys_info.freeswap/1024);
            add_assoc_long(mem_zval, "procs", sys_info.procs);
            add_assoc_long(mem_zval, "mem_unit", sys_info.mem_unit/1024);
        }
        add_assoc_zval(ret, "mem_data", mem_zval);
    }

    if(is_cpu){
        zval *zval_cpu;
        MAKE_STD_ZVAL(zval_cpu);
        array_init(zval_cpu);

        FILE *fp;
        char buff[256];
        cpu_data *cpudata;
        cpudata = emalloc(sizeof(cpu_data));
        fp = fopen("/proc/stat","r");
        fgets(buff,sizeof(buff),fp);
         
        WRITE_LOG("get_system_info:%s\n",buff);
        sscanf(buff,"%s %u %u %u %u %u %u %u", cpudata->name, &cpudata->user, &cpudata->nice, &cpudata->system, &cpudata->idle, &cpudata->iowait, &cpudata->irq, &cpudata->softirq);
           
        add_assoc_string(zval_cpu, "name", cpudata->name, 1);
        add_assoc_long(zval_cpu, "user", cpudata->user);
        add_assoc_long(zval_cpu, "nice", cpudata->nice);
        add_assoc_long(zval_cpu, "system", cpudata->system);
        add_assoc_long(zval_cpu, "idle", cpudata->idle);
        add_assoc_long(zval_cpu, "iowait", cpudata->iowait);
        add_assoc_long(zval_cpu, "irq", cpudata->irq);
        add_assoc_long(zval_cpu, "softirq", cpudata->softirq);
        add_assoc_zval(ret, "cpu_data", zval_cpu);
    }
	
    RETURN_ZVAL(ret,1,1);
}
/* }}} */

/* get request health status
 * {{{ get_req_status
 */
PHP_FUNCTION(get_req_status)
{
    zval *ret;
    char *key;
    int key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
        return;
    }
    
    MYMONITOR_G(request_status)->starttime++;
    //php_printf("mymonitor_G(starttime):%d",MYMONITOR_G(request_status)->starttime);

    MAKE_STD_ZVAL(ret);
    array_init(ret);

    zend_rsrc_list_entry *le; 
    char *hash_key;
    int hash_key_len= spprintf(&hash_key,0,"%s",key);
    int res = zend_hash_find(&EG(persistent_list), hash_key, hash_key_len + 1, (void**)&le);
    if(res == SUCCESS){
        //共享内存的数据
        add_assoc_long(ret,"starttime",((req_status*)le->ptr)->shm_p->addr->starttime);
        add_assoc_string(ret,"stat_id",((req_status*)le->ptr)->shm_p->addr->stat_id, 1);

        http_code_dstat *code_arr = ((req_status*)le->ptr)->shm_p->addr->code_arr;
        while(code_arr->code != NULL){
            add_assoc_long(ret,code_arr->code,code_arr->num);
            code_arr++;
        }
        WRITE_LOG("[%d]: 方法所在的进程号 [%d] \n",__LINE__, getpid());

        //持久化的数据
        req_status *tmp_persist = le->ptr;
        http_code_dstat *code_arr_persist = ((req_status*)le->ptr)->code_arr;
        
        /*while(code_arr_persist->code != NULL){
            WRITE_LOG("持久化的CODE_ARR: code [%s], num [%ld] \n",code_arr_persist->code,code_arr_persist->num);
            code_arr_persist++;
        }*/

        efree(hash_key);
        RETURN_ZVAL(ret,1,1);
    }else{
        efree(hash_key);
        RETURN_FALSE;
    }

}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(mymonitor)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "mymonitor support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_mymonitor_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_mymonitor_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "mymonitor", arg);
	RETURN_STRINGL(strg, len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
