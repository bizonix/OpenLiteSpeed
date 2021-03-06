/*
Copyright (c) 2014, LiteSpeed Technologies Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met: 

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer. 
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution. 
    * Neither the name of the Lite Speed Technologies Inc nor the
      names of its contributors may be used to endorse or promote
      products derived from this software without specific prior
      written permission.  

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
/**
 * HOW TO TEST
 * /testsuspend/..... to set to suspend, and ?11 to set to handle
 * 
 */

#define _GNU_SOURCE 
#include "../../include/ls.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <memory.h>

#define     MNAME       testsuspend
extern lsi_module_t MNAME;

typedef struct _MyMData {
    void * notifier_pointer;
} MyMData;

int httpRelease(void *data)
{
    MyMData *myData = (MyMData *)data;
    if (myData)
    {
        g_api->remove_event_notifier( &myData->notifier_pointer);
        free( myData);
    }
        

    return 0;
}

void timer_callback(void* session)
{
    MyMData *myData = (MyMData *) g_api->get_module_data((lsi_session_t  *)session, &MNAME, LSI_MODULE_DATA_HTTP);
    if (myData == NULL)
        return;
    
    int len;
    const char *qs = g_api->get_req_query_string((lsi_session_t* )session, &len);
    if (len > 1 && strstr(qs, "11"))
        g_api->register_req_handler((lsi_session_t *)session, &MNAME, 12);
    
    g_api->notify_event_notifier(&myData->notifier_pointer);
}

void *thread_callback(void* session)
{
    sleep(5);
    timer_callback(session);
    pthread_exit(NULL);
}

int suspendFunc(lsi_cb_param_t *rec)
{
    const char *uri;
    int len;
    uri = g_api->get_req_uri(rec->_session, &len);
    if ( len >= 12 && strcasestr(uri, "/testsuspend"))
    {
        MyMData *myData = (MyMData *) g_api->get_module_data(rec->_session, &MNAME, LSI_MODULE_DATA_HTTP);
        if (!myData)
        {
            myData = (MyMData *)malloc(sizeof(MyMData));
            g_api->set_module_data(rec->_session, &MNAME, LSI_MODULE_DATA_HTTP, (void *)myData);
        }
        myData->notifier_pointer = 0;
        
        
// //#define USE_TIMER_HERE
// #ifdef  USE_TIMER_HERE
//         g_api->set_timer(5000, timer_callback, (void *)rec->_session);
//         return LSI_RET_SUSPEND;
// #else     
//         pthread_t mythread;
//         int rc = pthread_create(&mythread, NULL, thread_callback,  (void *)rec->_session);
//         if (rc == 0)
//         {
//             return LSI_RET_SUSPEND; //If ret LSI_RET_SUSPEND, should set a timer or a thread to call hookResumeCallback()
//         }
// #endif

        pthread_t mythread;
        myData->notifier_pointer = g_api->set_event_notifier(rec->_session, &MNAME, LSI_HKPT_RECV_REQ_HEADER);
        if (myData->notifier_pointer) 
        {
            int rc = pthread_create(&mythread, NULL, thread_callback,  (void *)rec->_session);
            if (rc == 0)
            {
                return LSI_RET_SUSPEND; //If ret LSI_RET_SUSPEND, should set a timer or a thread to call hookResumeCallback()
            }
        }
    }
    
    return LSI_RET_OK;
}

static int clearData(lsi_cb_param_t *rec)
{
    g_api->free_module_data(rec->_session, &MNAME, LSI_MODULE_DATA_HTTP, httpRelease);
    return 0;
}


static lsi_serverhook_t serverHooks[] = {
    {LSI_HKPT_RECV_REQ_HEADER, suspendFunc, LSI_HOOK_NORMAL, LSI_HOOK_FLAG_ENABLED},
    {LSI_HKPT_HTTP_END, clearData, LSI_HOOK_LAST, LSI_HOOK_FLAG_ENABLED},
    lsi_serverhook_t_END   //Must put this at the end position
};

static int _init(lsi_module_t * pModule)
{
    g_api->init_module_data(pModule, httpRelease, LSI_MODULE_DATA_HTTP );
    return 0;
}


//The first time the below function will be called, then onWriteEvent will be called next and next
static int myhandler_process(lsi_session_t *session)
{
    char tmBuf[30];
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = gmtime(&t);
    strftime( tmBuf, 30, "%a, %d %b %Y %H:%M:%S GMT", tmp );
    g_api->set_resp_header(session, LSI_RESP_HEADER_CONTENT_TYPE, NULL, 0, "text/html", sizeof("text/html") - 1, LSI_HEADER_SET );
    g_api->set_resp_header(session, LSI_RESP_HEADER_LAST_MODIFIED, NULL, 0, tmBuf, 29, LSI_HEADER_SET);
    
    char buf[1024];
    sprintf(buf, "This test suspend for 5 seconds and resume, now time is %ld<p>", (long)(time(NULL)));
    g_api->append_resp_body(session, buf, strlen(buf));
    g_api->end_resp(session);
    return 0;
}

lsi_handler_t myhandler = { myhandler_process, NULL, NULL, NULL };
lsi_module_t MNAME = { LSI_MODULE_SIGNATURE, _init, &myhandler, NULL, "", serverHooks};
