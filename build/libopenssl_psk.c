#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

int main (void)
{
    SSL_CTX ctx;

    /* check PSK */
    SSL_CTX_set_psk_client_callback(&ctx, NULL);

    return 0;
}
