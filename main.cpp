#include "Classes/Server_Class.h"
#define NUMBER_CONCURRENT_CLIENTS 10
//tried http but fail to bind
#define SERVER_PORT "1110"



int main() {
    Server_Class server(SERVER_PORT, NUMBER_CONCURRENT_CLIENTS);
    server.build_and_run_server();
    return 0;
}
/**/