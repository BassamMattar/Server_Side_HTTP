//
// Created by bassammattar on ٢٧‏/١٠‏/٢٠١٩.
//

#ifndef SERVER_SIDE_HTTP_SERVER_CLASS_H
#define SERVER_SIDE_HTTP_SERVER_CLASS_H

#include "../General/UTILS.h"
#include <iostream>
#include <iostream>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <atomic>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <sys/sendfile.h>
#include <fcntl.h>
using namespace std;
class Server_Class {
private:
    char* server_port;
    int server_socket = -1, max_socket, server_max_client;
    fd_set server_set, read_set;//sets to use select sockets
    static atomic<int> current_clients;
    static void handle_client(int client_socket, int in_active_duration_milli);
    int get_http_socket(char* port);
    int prepare_acceptance_phase(int max_client);
    void server_loop();
    static void send(int client_socket, string http_header, string http_response);
    static void send(int client_socket, string http_header, char file_buffer[], int buffer_length);
public:
    Server_Class(char *port, int max_client);
    void build_and_run_server();
};
inline atomic<int> Server_Class::current_clients;

#endif //SERVER_SIDE_HTTP_SERVER_CLASS_H
