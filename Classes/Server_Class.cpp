//
// Created by bassammattar on ٢٧‏/١٠‏/٢٠١٩.
//

#include "Server_Class.h"
using namespace std;
Server_Class::Server_Class(char *port, int max_client) {
    this->server_port = port;
    this->server_max_client = max_client;
    current_clients = 0;
    atomic_init(&current_clients, 0);
}

int Server_Class::get_http_socket(char* port) {
    //Prepare socket information about service
    int reuse_socket = 1;//used to reuse sockets after close server loop
    struct addrinfo info_about_addr; //information to create addrinfo used in getaddrinfo
    struct addrinfo *result_service_info;//result got by getaddrinfo about the service server provide
    memset(&info_about_addr, 0, sizeof info_about_addr);//clear carbage values of allocation
    info_about_addr.ai_family = AF_UNSPEC; // using both IPv4 or IPv6
    info_about_addr.ai_socktype = SOCK_STREAM;//using TCP protocol
    info_about_addr.ai_flags = AI_PASSIVE; // use machine IP directly
    int received_code;//used for tracking error
    if ((received_code = getaddrinfo(nullptr, port, &info_about_addr, &result_service_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(received_code));//debug error
        return -1;//exit server
    }
    struct addrinfo *addrinfo_item;
    // loop through all the results and bind to the first correct allocated socket
    for(addrinfo_item = result_service_info; addrinfo_item != nullptr; addrinfo_item = addrinfo_item->ai_next) {
        if ((this->server_socket = socket(addrinfo_item->ai_family, addrinfo_item->ai_socktype,
                                          addrinfo_item->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        // allow server re-usage
        setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(int));

        if (bind(this->server_socket, addrinfo_item->ai_addr, addrinfo_item->ai_addrlen) == -1) {
            close(this->server_socket);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (addrinfo_item == nullptr) {
        fprintf(stderr, "server: failed to bind socket\n");
        return -1;
    }

    freeaddrinfo(result_service_info);//result list not needed any more we finally got the socket
    printf("Server: socket is bound successfully...\n");
    return this->server_socket;
}

int Server_Class::prepare_acceptance_phase(int max_clients) {
    // listen
    if (listen(this->server_socket, max_clients) == -1) {
        perror("listen");
        exit(3);
    }
    printf("Server: Server is listening on http port...\n");
    //begin accept requests for connection
    this->max_socket = this->server_socket;
    FD_ZERO(&this->server_set);//clear carbage allocation
    FD_ZERO(&this->read_set);
    FD_SET(this->server_socket, &server_set);//add server socket to be used among clients to handle(accept) incoming requests
    return  0;
}

void Server_Class::server_loop() {
    struct sockaddr_storage client_addr;//incoming client information
    socklen_t addr_len;
    char client_IP[INET6_ADDRSTRLEN];//used to include IP for incoming clients
    while(1) {
        read_set = this->server_set; // copy all clients sockets to active set to be filtered later
        if (select(this->max_socket+1, &read_set, NULL, NULL, NULL) == -1) {//filtering sockets
            perror("select");
            exit(4);
        }
        // run through the filtered sockets to handle them
        for(int i = 0; i <= this->max_socket; i++) {
            if (FD_ISSET(i, &read_set)) { // socket belong to filtered set
                // handle new connections
                addr_len = sizeof client_addr;
                int new_server_to_client_socket = accept(this->server_socket, (struct sockaddr *)&client_addr, &addr_len);
                if (new_server_to_client_socket == -1) {
                    perror("accept");
                } else {
                    //FD_SET(new_server_to_client_socket, &all_clients_set); // add to all client set to be tracked later
                    if (new_server_to_client_socket > this->max_socket) {    // keep track of the max
                        this->max_socket = new_server_to_client_socket;
                    }
                    printf("Server: Server accept new connection from %s on socket %d\n",
                           inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), client_IP, INET6_ADDRSTRLEN), new_server_to_client_socket);
                    current_clients++;
                    int time_out = 100000;//TODO determine it
                    thread(Server_Class::handle_client, new_server_to_client_socket, time_out).detach();

                }

            }

        }
    }
}

void Server_Class::handle_client(int client_socket, int in_active_duration_milli) {//thread function
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
            if(request.find("GET")!= request.npos) {
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
                    http_header =   "HTTP/1.1 200 OK\r\n"
                                    "Content-length: "+to_string(response.length()) +"\r\n"
                                    "Connection: Keep-Alive\r\n"
                                    "Content-Type: text/html; charset=UTF-8\r\n\r\n";
                    send(client_socket, http_header, response);
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
                    send(client_socket, http_header, response);
                } else {
                    path = "." + path;
                    fstream http_file(path);//get file binary data
                    http_file.seekg(0, std::fstream::end);
                    size_t file_buffer_length = http_file.tellg();//get length of file
                    http_file.seekg(0, std::fstream::beg);
                    char file_buffer[file_buffer_length + 1];//make buffer of length of file
                    http_file.read(file_buffer, file_buffer_length);//put binary data in buffer
                    string http_header;
                    if(!http_file.good()) {//not existed
                        http_header = "HTTP/1.1 404 Not Found\r\n"
                                      "Content-length: 0\r\n"
                                      "Content-Type: text/html; charset=UTF-8\r\n\r\n";

                    } else {
                        http_header =   "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: application/octet\r\n"
                                        "Content-Length: " + to_string(sizeof file_buffer) + "\r\n"
                                                                                            "\r\n";
                    }
                    //debug
                    cout << "BUFFER LENGTH: " << to_string(sizeof file_buffer) << endl;
                    cout << "RESPONSE LENGTH: " << to_string(sizeof file_buffer) << endl;
                    cout << http_header << endl;
                    send(client_socket, http_header, file_buffer, sizeof file_buffer);
                }
            } else if(request.find("POST")!= request.npos) {

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



void Server_Class::build_and_run_server() {
    get_http_socket(this->server_port);
    prepare_acceptance_phase(this->server_max_client);
    server_loop();
}

void Server_Class::send(int client_socket, string http_header, string http_response) {
    char header_chars[http_header.length() + 1];
    char response_chars[http_response.size() + 1];
    strcpy(header_chars, http_header.c_str());
    strcpy(response_chars, http_response.c_str());
    int header_char_length = http_header.length();
    int response_char_length = http_response.length();
    if (sendall(client_socket, header_chars, &header_char_length) != 0) {//check if there error or connection end
        perror("send");
    } else {//actual data is sent
        //debug data
        printf("response is sent successfully\n");
    }

    if (sendall(client_socket, response_chars, &response_char_length) != 0) {//check if there error or connection end
        perror("send");
    } else {//actual data is sent
        //debug data
        printf("response is sent successfully\n");
    }
}

void Server_Class::send(int client_socket, string http_header, char file_buffer[], int buffer_length) {
    char header_chars[http_header.length() + 1];
    strcpy(header_chars, http_header.c_str());
    int header_char_length = http_header.length();

    if (sendall(client_socket, header_chars, &header_char_length) != 0) {//check if there error or connection end
        perror("send");
    } else {//actual data is sent
        //debug data
        printf("response is sent successfully\n");
    }

    if (sendall(client_socket, file_buffer, &buffer_length) != 0) {//check if there error or connection end
        perror("send");
    } else {//actual data is sent
        //debug data
        printf("response is sent successfully\n");

    }
}
