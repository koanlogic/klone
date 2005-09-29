#include <openssl/crypto.h>

int main()
{
    long ver;

    ver = SSLeay();

    return 0;
}
