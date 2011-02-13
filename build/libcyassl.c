#define NO_FILESYSTEM /* needed for CyaSSL_CTX_load_verify_buffer */
#include <openssl/ssl.h>
#include <openssl/opensslv.h>

void SetErrorString(int , char* );

int main()
{
    SSL_CTX *c = 0;
    void* f = 0;

    /* CyaSSL specific fun */
    f = SetErrorString;

    /* defined if NO_FILESYSTEM is set */
    f = CyaSSL_CTX_load_verify_buffer;

    /* defined if --enable-opensslExtra CyaSSL option has been used */
    f = SSL_CTX_set_default_passwd_cb;

    /* defined in CyaSSL version of openssl/opensslv.h */
#ifndef CYASSL_OPENSSLV_H_
    printf("openssl/opensslv.h doesn't seem to be the one from CyaSSL\n");
    return 1;
#endif

    /* defined in CyaSSL version of openssl/ssl.h */
#ifndef CYASSL_OPENSSL_H_
    printf("openssl/ssl.h doesn't seem to be the one from CyaSSL\n");
    return 1;
#endif

    SSL_load_error_strings();
    SSL_library_init();

    (void*)SSL_CTX_new(SSLv23_server_method());
    
    return 0;
}
