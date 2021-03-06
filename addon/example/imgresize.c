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

#include "../include/ls.h"
#include <lsr/lsr_loopbuf.h>

#include "gd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/****************************************************************
 * How to test: Access an image dynamically (i.e. with a proxy),
 * ensuring that the resize dimensions are in the query string.
 * For example: http://localhost:8088/apple.jpeg?w=400&h=500
 * 
 ***************************************************************/

enum httpimg{ 
    HTTP_IMG_GIF, 
    HTTP_IMG_PNG,
    HTTP_IMG_JPEG
};

/////////////////////////////////////////////////////////////////////////////
//DEFINE the module name, MUST BE the same as .so file name
//ie.  if MNAME is testmodule, then the .so must be testmodule.so
#define     MNAME       imgresize
/////////////////////////////////////////////////////////////////////////////

lsi_module_t MNAME;
#define MAX_BLOCK_BUFSIZE   8192

typedef struct _MyData
{
    lsr_loopbuf_t inWBuf;
    lsr_loopbuf_t outWBuf;
    enum httpimg IMAGE_TYPE;
} MyData;

/*Function Declarations*/
static int setWaitFull( lsi_cb_param_t * rec );
static int scanForImage( lsi_cb_param_t * rec );
static int parseParameters( lsi_cb_param_t * rec, MyData *myData );
static int getReqDimensions( const char *buf, int iLen, int *width, int *height );
static void* resizeImage( const char *buf, int bufLen, int width, int height, MyData *myData, int *size );
static int _init();

int httpRelease(void *data)
{
    g_api->log( NULL, LSI_LOG_DEBUG, "#### mymoduleresize %s\n", "httpRelease" );
    return 0;
}

int httpinit(lsi_cb_param_t * rec)
{
    MyData *myData;
    lsr_xpool_t *pool = g_api->get_session_pool( rec->_session );
    myData = lsr_xpool_alloc( pool, sizeof( MyData ));
    lsr_loopbuf_x( &myData->inWBuf, MAX_BLOCK_BUFSIZE, pool );
    lsr_loopbuf_x( &myData->outWBuf, MAX_BLOCK_BUFSIZE, pool );
    
    g_api->log( NULL, LSI_LOG_DEBUG, "#### mymoduleresize init\n" );
    g_api->set_module_data(rec->_session, &MNAME, LSI_MODULE_DATA_HTTP, (void *)myData);
    return 0;
}

static int setWaitFull( lsi_cb_param_t * rec )
{
    g_api->set_resp_wait_full_body( rec->_session );
    return LSI_RET_OK;
}

/* Returns 0 for image, 1 for wrong input */
static int parseParameters( lsi_cb_param_t * rec, MyData *myData )
{
    int iLen;
    const char *ptr;
    struct iovec iov;
    
    if ( (iLen = g_api->get_resp_header( rec->_session, LSI_RESP_HEADER_CONTENT_TYPE, NULL, 0, &iov, 1 )) < 1 
        || iov.iov_len < 9
    )
        return 1;
    
    ptr = (const char *)iov.iov_base;
    if ( memcmp( ptr, "image/", 6 ) != 0 )
        return 1;
    
    if ( memcmp( ptr + 6, "png", 3 ) == 0 )
        myData->IMAGE_TYPE = HTTP_IMG_PNG;
    else if ( memcmp( ptr + 6, "gif", 3 ) == 0 )
        myData->IMAGE_TYPE = HTTP_IMG_GIF;
    else if ( memcmp( ptr + 6, "jpeg", 4 ) == 0 )
        myData->IMAGE_TYPE = HTTP_IMG_JPEG;
    else
        return 1;
    return 0;
}

/*return 0 on success, -1 for garbage queries*/
static int getReqDimensions( const char *buf, int iLen, int *width, int *height )
{
    const char *parse, *ptr2, *ptr = buf, *pEnd = buf + iLen;
    int val = 0;
    while( ptr < pEnd - 1 )
    {
        if ( *ptr != 'w' && *ptr != 'h' )
            return -1;
        if ( *(ptr + 1) != '=' )
            return -1;
        ptr2 = memchr( ptr + 2, '&', (pEnd - ptr) - 2 );
        if ( !ptr2 )
            ptr2 = pEnd;
        for( parse = ptr + 2; parse < ptr2; ++parse )
        {
            if ( *parse < '0' || *parse > '9' )
                return -1;
            val = val * 10 + (*parse - '0');
        }
        if ( *ptr == 'w' && *width == 0 )
            *width = val;
        else if ( *ptr == 'h' && *height == 0 )
            *height = val;
        if ( *ptr2 != '&' )
            return 0;
        val = 0;
        ptr = ptr2 + 1;
    }
    return 0;
}

static void *resizeImage( const char *buf, int bufLen, int width, int height, MyData *myData, int *size )
{
    
    char *ptr;
    gdImagePtr dest, src;
    if ( myData->IMAGE_TYPE == HTTP_IMG_JPEG )
        src = gdImageCreateFromJpegPtr( bufLen, (void *)buf );
    else if ( myData->IMAGE_TYPE == HTTP_IMG_GIF )
        src = gdImageCreateFromGifPtr( bufLen, (void *)buf );
    else if ( myData->IMAGE_TYPE == HTTP_IMG_PNG )
        src = gdImageCreateFromPngPtr( bufLen, (void *)buf );
    else
        return NULL;
    
    if ( !width && !height )
        return NULL;
    else if ( !width )
        width = height * src->sx / src->sy;
    else if ( !height )
        height = width * src->sy / src->sx;
    dest = gdImageCreateTrueColor( width, height );
    
    gdImageCopyResampled( dest, src, 0, 0, 0, 0, 
                          width, height, 
                          src->sx, src->sy );
    
    if ( myData->IMAGE_TYPE == HTTP_IMG_JPEG )
        ptr = gdImageJpegPtr( dest, size, 50 );
    else if ( myData->IMAGE_TYPE == HTTP_IMG_GIF )
        ptr = gdImageGifPtr( dest, size );
    else if ( myData->IMAGE_TYPE == HTTP_IMG_PNG )
        ptr = gdImagePngPtr( dest, size );
    
    return ptr;
}

static int scanForImage( lsi_cb_param_t * rec )
{
    off_t offset = 0;
    int iSrcSize, iDestSize, iWidth = 0, iHeight = 0, iLen = 0;
    void *pRespBodyBuf, *pSrcBuf, *pDestBuf;
    const char *ptr, *pDimensions;
    MyData *myData = (MyData *)g_api->get_module_data(rec->_session, &MNAME, LSI_MODULE_DATA_HTTP);
    lsr_xpool_t *pPool = g_api->get_session_pool( rec->_session );
    
    if ( parseParameters( rec, myData ) == 0 )
    {
        
        pDimensions = g_api->get_req_query_string( rec->_session, &iLen );
        if ( (iLen == 0)
            || (getReqDimensions( pDimensions, iLen, &iWidth, &iHeight ) != 0)
        )
        {
            return LSI_RET_OK;
        }
        pRespBodyBuf = g_api->get_resp_body_buf( rec->_session );
        if ( !pRespBodyBuf )
            return LSI_RET_OK;
        iSrcSize = g_api->get_body_buf_size( pRespBodyBuf );
        pSrcBuf = lsr_xpool_alloc( pPool, iSrcSize );
        while( !g_api->is_body_buf_eof( pRespBodyBuf, offset ) )
        {
            ptr = g_api->acquire_body_buf_block( pRespBodyBuf, offset, &iLen );
            if ( !ptr )
                break;
            memcpy( pSrcBuf + offset, ptr, iLen );
            g_api->release_body_buf_block( pRespBodyBuf, offset );
            offset += iLen;
        }
    }
    else
    {
        return LSI_RET_OK;
    }
    g_api->reset_body_buf( pRespBodyBuf );
    if ( g_api->append_body_buf( pRespBodyBuf, pSrcBuf, iSrcSize ) != iSrcSize )
        return LSI_RET_ERROR;
    iSrcSize = g_api->get_body_buf_size( pRespBodyBuf );
    
    pDestBuf = resizeImage( pSrcBuf, iSrcSize, iWidth, iHeight, myData, &iDestSize );
    if ( !pDestBuf )
        return LSI_RET_ERROR;    
    
    while( g_api->is_resp_buffer_available( rec->_session ) <= 0 );
    
    
    g_api->reset_body_buf( pRespBodyBuf );
    if ( g_api->append_body_buf( pRespBodyBuf, pDestBuf, iDestSize ) != iDestSize )
        return LSI_RET_ERROR;
    g_api->set_resp_content_length( rec->_session, iDestSize );
    
    gdFree( pDestBuf );
    return LSI_RET_OK;
}

static lsi_serverhook_t serverHooks[] = {
    {LSI_HKPT_HTTP_BEGIN, httpinit, LSI_HOOK_NORMAL, LSI_HOOK_FLAG_ENABLED},
    {LSI_HKPT_RECV_REQ_HEADER, setWaitFull, LSI_HOOK_NORMAL, LSI_HOOK_FLAG_ENABLED},
    {LSI_HKPT_SEND_RESP_HEADER, scanForImage, LSI_HOOK_NORMAL, LSI_HOOK_FLAG_ENABLED},
    lsi_serverhook_t_END   //Must put this at the end position
};

static int _init( lsi_module_t *pModule )
{
    g_api->init_module_data(pModule, httpRelease, LSI_MODULE_DATA_HTTP );
    return 0;
}

lsi_module_t MNAME = { LSI_MODULE_SIGNATURE, _init, NULL, NULL, "imgresize", serverHooks};






