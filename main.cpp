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

#define NUMBER_CONCURRENT_CLIENTS 10
//tried http but fail to bind
#define SERVER_PORT "1110"

using namespace std;

atomic<int> current_clients;
// get sockaddr, IPv4 or IPv6 -> provide portability
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
// handle partial sneding
int sendall(int s, char *buf, int *len)
{
    int total = 0;
    // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}
string get_file_content(string path) {

}
void handle_client(int client_socket, int in_active_duration_milli) {//thread function
    chrono::steady_clock::time_point timer = chrono::steady_clock::now() + chrono::milliseconds(in_active_duration_milli);//using timer
    fd_set client_set, read_filtered_set, write_filtered_set;
    FD_ZERO(&client_set);
    FD_SET(client_socket, &client_set);
    printf("Socket %d is being served\n", client_socket);
    char buf[512];
    int nbytes;
    int request_data = 0;
    while(chrono::steady_clock::now() < timer) {
        read_filtered_set = client_set;
        write_filtered_set = client_set;
        if (select(client_socket+1, &read_filtered_set, &write_filtered_set, NULL, NULL) == -1) {//filtering sockets
            perror("select");
            break;
        }
        if(FD_ISSET(client_socket, &read_filtered_set)) {
            if ((nbytes = recv(client_socket, buf, sizeof buf, 0)) <= 0) {//check if there error or connection end
                // got error or connection closed by client
                if (nbytes == 0) {
                    // connection closed
                    printf("Server: socket %d hung up\n", client_socket);
                } else {
                    perror("recv");
                }
                break;
            } else {
                printf("reset timer for socket %d\n", client_socket);
                timer =  chrono::steady_clock::now() + chrono::milliseconds(in_active_duration_milli);//reset timer
                printf("Socket %d: request sent\n", client_socket);
                printf("Socket %d: %s\n", client_socket, buf);
            }
        }
        if(FD_ISSET(client_socket, &write_filtered_set) && buf[0] != NULL) {
            string request(buf);
            if(request.find("GET")!=request.npos) {
                printf("Socket %d handles GET request.\n", client_socket);
                printf("reset timer for socket %d\n", client_socket);
                timer =  chrono::steady_clock::now() + chrono::milliseconds(in_active_duration_milli);//reset timer
                printf("begin sending data\n");
                int get_pos = request.find("GET");
                int http_pos = request.find("HTTP/1.1");
                string path = request.substr(get_pos + strlen("GET "), http_pos - get_pos - strlen("GET ") - 1);
                printf("Client requested path %s\n", path.c_str());
                string response = "";
                string http_header = "";
                if(path == "/") {
                    response =  "<!DOCTYPE html><html><body><h1>Hello World!</h1></body></html>";
                    http_header = "HTTP/1.1 200 OK\r\n"
                                  "Content-length: "+to_string(response.length()) +"\r\n"
                                   "Connection: Keep-Alive\r\n"
                                   "Content-Type: text/html; charset=UTF-8\r\n\r\n";
                } else if(path.find(".html") != path.npos) {
                    path = "." + path;
                    ifstream f(path.c_str());
                    if(!f.good()) {//not existed
                        http_header = "HTTP/1.1 404 Not Found\r\n"
                                      "Content-length: "+to_string(response.length()) +"\r\n"
                                      "Content-Type: text/html; charset=UTF-8\r\n\r\n";;
                    } else {
                        stringstream strStream;
                        strStream << f.rdbuf();
                        response = strStream.str();
                        http_header =   "HTTP/1.1 200 OK\r\n"
                                        "Content-length: "+to_string(response.length()) +"\r\n"
                                        "Connection: Keep-Alive\r\n"
                                        "Content-Type: text/html; charset=UTF-8\r\n\r\n";
                    }
                } else if(path.find(".jpeg") != path.npos) {
                    http_header =   "HTTP/1.1 200 Ok\r\n"
                                    "Content-length: 0\r\n"
                                    "Content-Type: image/jpeg\r\n\r\n";
                    char response_http[http_header.size() + 1];
                    strcpy(response_http, http_header.c_str());
                    write(client_socket, response_http, sizeof(response_http) - 1);
                    int fdimg = open(path.c_str(), O_RDONLY);
                    int sent = sendfile(client_socket, fdimg, NULL, 11875);//TODO check file size
                    printf("sent: %d\n", sent);
                    close(fdimg);
                    buf[0] = NULL;
                    continue;
                }
                //TODO handle different types html and images
                response = http_header + response;
                char response_chars[response.size() + 1];
                strcpy(response_chars, response.c_str());
                int total_len = sizeof response_chars;

                if (sendall(client_socket, response_chars, &total_len) != 0) {//check if there error or connection end
                    perror("send");
                } else {//actual data is sent
                    //debug data
                    printf("response is sent successfully\n");

                }
            }
            //TODO handle post request
            buf[0] = NULL;
        }
    }
    printf("Socket %d is being closed(Time_out)\n", client_socket);
    close(client_socket);
    current_clients--;
    pthread_exit(0);
}

int main() {
    //Prepare socket information about service
    int server_first_socket;//socket that will be listening
    int reuse_socket = 1;//used to reuse sockets after close server loop
    struct addrinfo info_about_addr; //information to create addrinfo used in getaddrinfo
    struct addrinfo *result_service_info;//result got by getaddrinfo about the service server provide
    memset(&info_about_addr, 0, sizeof info_about_addr);//clear carbage values of allocation
    info_about_addr.ai_family = AF_UNSPEC; // using both IPv4 or IPv6
    info_about_addr.ai_socktype = SOCK_STREAM;//using TCP protocol
    info_about_addr.ai_flags = AI_PASSIVE; // use machine IP directly

    int received_code;//used for tracking error
    if ((received_code = getaddrinfo(NULL, SERVER_PORT, &info_about_addr, &result_service_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(received_code));//debug error
        return 1;//exit server
    }
    struct addrinfo *addrinfo_item;
    // loop through all the results and bind to the first correct allocated socket
    for(addrinfo_item = result_service_info; addrinfo_item != NULL; addrinfo_item = addrinfo_item->ai_next) {
        if ((server_first_socket = socket(addrinfo_item->ai_family, addrinfo_item->ai_socktype,
                                          addrinfo_item->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        // allow server re-usage
        setsockopt(server_first_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(int));

        if (bind(server_first_socket, addrinfo_item->ai_addr, addrinfo_item->ai_addrlen) == -1) {
            close(server_first_socket);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (addrinfo_item == NULL) {
        fprintf(stderr, "server: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(result_service_info);//result list not needed any more we finally got the socket
    printf("Server: socket is bound successfully...\n");
    // listen
    if (listen(server_first_socket, NUMBER_CONCURRENT_CLIENTS) == -1) {
        perror("listen");
        exit(3);
    }
    printf("Server: Server is listening on http port...\n");
    //begin accept requests for connection
    struct sockaddr_storage client_addr;//incoming client information
    socklen_t addr_len = sizeof client_addr;
    int max_socket_number = server_first_socket;//keep track of the max socket number to be able to iterate over
    int new_server_to_client_socket;//used to store the socket number of the new connection
    char client_IP[INET6_ADDRSTRLEN];//used to include IP for incoming clients
    char buf[256];    // buffer for client data
    int nbytes; // data already read from the server
    fd_set all_clients_set, active_set, write_active_set;//sets to use select sockets
    FD_ZERO(&all_clients_set);//clear carbage allocation
    FD_ZERO(&active_set);
    FD_ZERO(&write_active_set);
    FD_SET(server_first_socket, &all_clients_set);//add server socket to be used among clients to handle(accept) incoming requests
    //server loop
    while(1) {
        active_set = all_clients_set; // copy all clients sockets to active set to be filtered later
        write_active_set = all_clients_set;
        if (select(max_socket_number+1, &active_set, NULL, NULL, NULL) == -1) {//filtering sockets
            perror("select");
            exit(4);
        }

        // run through the filtered sockets to handle them
        for(int i = 0; i <= max_socket_number; i++) {
            if (FD_ISSET(i, &active_set)) { // socket belong to filtered set
                if (i == server_first_socket) {// is this the server? if yes there incoming connection
                    // handle new connections
                    addr_len = sizeof client_addr;
                    new_server_to_client_socket = accept(server_first_socket, (struct sockaddr *)&client_addr, &addr_len);
                    if (new_server_to_client_socket == -1) {
                        perror("accept");
                    } else {
                        //FD_SET(new_server_to_client_socket, &all_clients_set); // add to all client set to be tracked later
                        if (new_server_to_client_socket > max_socket_number) {    // keep track of the max
                            max_socket_number = new_server_to_client_socket;
                        }
                        printf("Server: Server accept new connection from %s on socket %d\n",
                               inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), client_IP, INET6_ADDRSTRLEN), new_server_to_client_socket);
                        int time_out = 100000;//TODO determine it
                        thread(handle_client, new_server_to_client_socket, time_out).detach();

                    }
                }
            }

        }
    }
    return 0;
}
/**/