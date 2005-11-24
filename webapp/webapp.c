#include <klone/io.h>
#include <klone/response.h>

void wa_test(response_t *response) 
{
    io_printf(response_io(response), "Custom Web Application");
}
