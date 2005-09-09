#ifndef _KLONE_CGI_H_
#define _KLONE_CGI_H_
#include <klone/request.h>

int cgi_set_request(request_t *rq);


#if 0
typedef struct cgi_request_s
{
    #if 0 
    not req specific:
        SERVER_SOFTWARE     name/ver
        SERVER_NAME         hostname/ip
        GATEWAY_INTERFACE   CGI/rev

    req specific
        SERVER_PROTOCOL     proto/ver
        SERVER_PORT         NN
        REQUEST_METHOD      "GET", "POST", etc.
        PATH_INFO           extra info at the end of the path in the url
        PATH_TRASLATED      physical path of the requested file
        SCRIPT_NAME         virtual path of the script
        QUERY_STRING        info following the ? in the url
        REMOTE_HOST         remote hostname (may be blank)
        REMOTE_ADDR         remote ip
        AUTH_TYPE           auth method used
        REMOTE_USER         authenticated user
        REMOTE_IDENT        user name (see rfc931)
        CONTENT_TYPE        content type 
        CONTENT_LENGTH      content length
        HTTP_xxx            additional header set by the client (tr/-/_/)
                            for ex. HHTP_ACCESS, HTTP_USER_AGENT
    #endif
    #if 0
    CGI env to request_t mapping:
        SCRIPT_NAME     -->     uri
        PATH_TRANSLATED -->     filename
        PATH_INFO       -->     path_info
        QUERY_STRING    -->     args
        SERVER_PROTOCOL -->     protocol
        REQUEST_METHOD  -->     method
    #endif
    char *cgi_revision; /* cgi specific */
	char *path_info, *path_translated, *script_name, *query_string;;
	char *auth_type, *remote_user, *remote_ident;
} cgi_request_t;
#endif

#endif
