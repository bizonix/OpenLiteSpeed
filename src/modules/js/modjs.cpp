/*
Copyright (c) 2013, LiteSpeed Technologies Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met: 

    * Redistributions of source code must retain the above copyright
      notice, this list of condiions and the following disclaimer. 
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
#include "ls.h"
#include "lsjsengine.h"
#include <string.h>
#include <unistd.h>
#include <time.h>

#define     MNAME       mod_js
extern lsi_module_t MNAME;

/* Module Data */
typedef struct {
    void * pSession;
} jsModuleData_t;

static int releaseJsData( void * data)
{
    if (!data)
        return 0;
    jsModuleData_t * pData = (jsModuleData_t *) data;

    pData->pSession = NULL;
    free(pData);
    return 0;
}

static jsModuleData_t * allocateJsData(lsi_session_t * session, const lsi_module_t * module, int level)
{
    jsModuleData_t * pData = (jsModuleData_t *) malloc(sizeof(jsModuleData_t));
    if (!pData)
        return NULL;
    memset(pData, 0, sizeof(jsModuleData_t));
    g_api->set_module_data(session, module, level, pData);
    return pData;
}

static void * getLsJsSession_from_moduleData(lsi_session_t * pSess)
{
    jsModuleData_t * pData = (jsModuleData_t *) g_api->get_module_data(pSess, &MNAME, LSI_MODULE_DATA_HTTP);
    if (pData)
        return pData->pSession;
    else
        return NULL;
}

#if 0
static int http_end(lsi_cb_param_t *rec)
{
    extern void http_end_callback(void *);

    g_api->log( NULL, LSI_LOG_DEBUG,  "HTTP_END [%s]\n", g_api->get_req_uri(rec->_session, NULL));
    // http_end_callback(rec->_session);
    return LSI_RET_OK;
}

static int http_begin(lsi_cb_param_t *rec)
{
    //const char *uri;
    //uri = g_api->get_req_uri(rec->_session, NULL );
    // g_api->log( NULL, LSI_LOG_DEBUG,  "HTTP_BEGIN [%s]\n", uri);
    return LSI_RET_OK;
}

static int recv_resp_header(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "RECV_RESP_HEADER [%s]\n", uri);
    return LSI_RET_OK;
}

static int recv_resp_body(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "RECV_RESP_BODY [%s]\n", uri);
    return LSI_RET_OK;
}

static int recvd_resp_body(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "RECVD_RESP_BODY [%s]\n", uri);
    return LSI_RET_OK;
}

static int send_resp_header(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "SEND_RESP_HEADER [%s]\n", uri);
    return LSI_RET_OK;
}

static int send_resp_body(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "SEND_RESP_BODY [%s]\n", uri);
    return LSI_RET_OK;
}

static int recved_req_handler(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "dummy_handler [%s]\n", uri);
    return LSI_RET_OK;
}

//
//  LSI_HKPT_RECV_REQ_BODY 
//
int recv_req_body(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "RECV_REQ_BODY JS exec_handler [%s]\n", uri);
    return LSI_RET_OK;
}

//
//  LSI_HKPT_RECV_REQ_HEADER 
//      If we choose to use HOOK POINT for handler selection,
//          this is the HOOK POINT...
//
int recv_req_header(struct lsi_cb_param_t *rec)
{
    const char *uri;
    uri = g_api->get_req_uri(rec->_session);
    g_api->log( NULL, LSI_LOG_DEBUG,  "RECV_REQ_HEADER JS [%s]\n", uri);
    if ( memcmp(uri, "/lsjs/", 7) == 0 )
    {
        if (LsJsEngine::isReady(rec->_session))
        {
            g_api->log( NULL, LSI_LOG_DEBUG,  "JS recv_req_header [%s]\n", uri);
            g_api->register_req_handler(rec->_session, &MNAME, 0);
            // This enforce the handler wont be called until all the data is there.
            // g_api->set_wait_full_req_body( rec->_session );
        }
    }
    return LSI_RET_OK;
}
#endif

static int _init( lsi_module_t * pModule )
{
    //
    //  load JS Engine here
    //
    if (LsJsEngine::init())
    {
        g_api->log( NULL, LSI_LOG_ERROR,  "FAILED TO SETUP Node.js interface\n");
        return -1;
    }
    pModule->_info = "Node.js Interface";  //set version string
    
    //
    // enable call back here ... if any
    //

    //
    //  module data for LsJsSession look up
    //
    g_api->init_module_data( pModule, releaseJsData, LSI_MODULE_DATA_HTTP );
    return 0;
}

static int jsHandler(lsi_session_t *session )
{
    jsModuleData_t * pData;

    pData = (jsModuleData_t *) g_api->get_module_data(session, &MNAME, LSI_MODULE_DATA_HTTP);
    if (!pData)
    {
        pData = allocateJsData(session, &MNAME, LSI_MODULE_DATA_HTTP);
        if (!pData)
        {
            g_api->log( NULL, LSI_LOG_ERROR,  "FAILED TO ALLOCATE MODULE DATA\n");
            return LSI_RET_ERROR;
        }
    }
    pData->pSession = NULL;

    char * uri;
    uri = (char *)g_api->get_req_uri( session, NULL );
    
#define MAXFILENAMELEN  0x1000
    char jsfile[MAXFILENAMELEN];   /* 4k filenamepath */
    char buf[0x1000];
    register int    n;

    if (g_api->is_req_body_finished(session))
    {
        ;
    }
    else
    {
        g_api->set_req_wait_full_body( session );
    }

    if (g_api->get_uri_file_path( session, uri, strlen(uri), jsfile, MAXFILENAMELEN))
    {
        n = snprintf(buf, 0x1000, "jshandler: FAILED TO COMPOSE JS script path %s\r\n", uri);
        g_api->append_resp_body( session, buf, n);
        g_api->end_resp( session );
        return LSI_RET_OK;
    }
    // set to 1 = continueWrite, 0 = suspendWrite - the default is 1 if onWrite is provied.
    // So I just set this off... 
    g_api->set_handler_write_state( session, 0);

    LsJsUserParam * pUser = (LsJsUserParam *) g_api->get_module_param(session, &MNAME);
    if (LsJsEngine::runScript(session, pUser, jsfile))
        return LSI_RET_ERROR;
    return LSI_RET_OK;
}

//
//  onCleanupEvent - remove junk...
//
static int onCleanupEvent( lsi_session_t *session )
{
    // g_api->log( session, LSI_LOG_NOTICE,  "JS onCleanupEvent\n");
    return 0;
}

//
//  onReadEvent - activated when whole request body has been read by the server.
//
static int onReadEvent( lsi_session_t *session )
{
    void * pSession;

    pSession = getLsJsSession_from_moduleData(session);
    
    // pSession = LsJsSession::findByLsiSession(session);
    if (!pSession)
    {
        g_api->log( session, LSI_LOG_NOTICE,  "ERROR: JS onReadEvent Session NULL\n");
        return 0;
    }

    //
    // Code from David... we need to worry about this later
    // This check statement should be an assert instead!
    if (!g_api->is_req_body_finished(session))
    {
        char buf[8192];
#define     CONTENT_FORMAT  "<tr><td>%s</td><td>%s</td></tr><p>\r\n"
        snprintf(buf, 8192, CONTENT_FORMAT, "ERROR", "You must forget to set wait all req body!!!<p>\r\n");
        g_api->append_resp_body(session, buf, strlen(buf));
        return 0;
    }
    return 0;
} 

//
//  A typical handler should set write state to 0... no onWriteEvent
//    g_api->set_handler_write_state( session, 0);
//
//  For js script -
//      We turn on the onWriteEvent while there is no more buffer for us to write.
//      when we get call
//      (1) check if there is buffer available for us to continue.
//      (2) resume the yielded code. (session level)
//
static int onWriteEvent( lsi_session_t *session )
{
    void * pSession;
    pSession = getLsJsSession_from_moduleData(session);
    
    // pSession = LsJsSession::findByLsiSession(session);
    // g_api->log( session, LSI_LOG_NOTICE,  "JS onWrite %p <%p>", session, pSession);
    if (pSession)
    {
#if 0
        if (pSession->onWrite(session) == 1)
        {
            return LSI_WRITE_RESP_CONTINUE;
        }
#endif
        ;
    }
    return LSI_WRITE_RESP_FINISHED;
}

//
//  module parameters
//  nodepath -> where my node.js objects...
//
const char *myParam[] = {
    "nodepath", 
    NULL   //The last position must have a NULL to indicate end of the array
};

/**
 * Define a handler, need to provide a struct _handler_functions_st object, in which 
 * the first function pointer should not be NULL
 */
static lsi_handler_t lsjs_mod_handler = { jsHandler, onReadEvent, onWriteEvent , onCleanupEvent};

static lsi_config_t lsjs_mod_config = { LsJsEngine::parseParam, LsJsEngine::removeParam , myParam };

lsi_module_t MNAME = { LSI_MODULE_SIGNATURE, 
                        _init, 
                        &lsjs_mod_handler, 
                        &lsjs_mod_config,
                        "v1.0", 
                        NULL,
                        {0}
};
