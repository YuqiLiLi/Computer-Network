#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define QUEUE_LENGTH 10
#define RECV_BUFFER_SIZE 2048

void handle_client(int client_fd) {
    // Read the client's request
    char buffer[RECV_BUFFER_SIZE];
    ssize_t bytesRead;
    ssize_t totalBytesRead = 0;
    while (1) {
        bytesRead = recv(client_fd, buffer + totalBytesRead, sizeof(buffer) - totalBytesRead, 0);

        if (bytesRead == -1) {
            perror("recv");
            close(client_fd);
            return;
        }

        totalBytesRead += bytesRead;

        // Check if HTTP end of request indicator is received
        char *header_end = strstr(buffer, "\r\n\r\n");
        if (header_end != NULL) {
            break;  // End of request indicator found, exit the loop
        }
    }

    


    buffer[totalBytesRead] = '\0';



    
    // Parse the client's request using the provided library
    printf("Received request: %s\n", buffer);

    struct ParsedRequest *parsedRequest = ParsedRequest_create();

    if (ParsedRequest_parse(parsedRequest, buffer, totalBytesRead) < 0) {
    fprintf(stderr, "Failed to parse the request\n");
    fprintf(stderr, "Request content: %s\n", buffer);
    char *response = "HTTP/1.0 400 Bad Request\r\n\r\n";
    send(client_fd, response, strlen(response), 0);
    
    close(client_fd);
    ParsedRequest_destroy(parsedRequest);
    return;
}
    // if (ParsedRequest_parse(parsedRequest, path_start, strlen(path_start)) < 0) {
    //     fprintf(stderr, "Failed to parse the request\n");
    //     fprintf(stderr, "Request content: %s\n", buffer);
    //     close(client_fd);
    //     ParsedRequest_destroy(parsedRequest);
    //     return;
    // }

    printf("Parsed request method: %s\n", parsedRequest->method);
    printf("Parsed request host: %s\n", parsedRequest->host);

    char *path_start = strstr(buffer, " ");
    if (path_start != NULL) {
        path_start++; // 移到路径的开始
        char *path_end = strchr(path_start, ' ');
        if (path_end != NULL) {
            *path_end = '\0'; // 截断路径
        // path_start 现在包含了解析后的路径
        }
    }

    char *host_start = strstr(buffer, "//");
    if (host_start != NULL) {
        host_start += 2; // 移到主机的开始
        char *host_end = strchr(host_start, '/');
        if (host_end != NULL) {
            *host_end = '\0'; // 截断主机
        // host_start 现在包含了解析后的主机
    }
    }

// 输出截断后的请求行、主机和路径信息
    printf("Truncated request line: %s\n", path_start);
    printf("Parsed host: %s\n", host_start);
    
   


    // Extract necessary information from the parsed request
    char *host = parsedRequest->host;
    char *port = parsedRequest->port ? parsedRequest->port : "80";
    char *path = parsedRequest->path ? parsedRequest->path : "/";

    if(strcmp(parsedRequest->method, "GET") != 0) {
        char *response = "HTTP/1.0 501 Not Implemented\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
        close(client_fd);
        ParsedRequest_destroy(parsedRequest);
        return;
    }

    //--modify--
    if (strcmp(parsedRequest->version, "HTTP/1.1") == 0) {
        parsedRequest->version = strdup("HTTP/1.0");
    }
    ParsedHeader_set(parsedRequest, "Connection", "close");
    ParsedHeader_set(parsedRequest, "Host", host);

    

    size_t total_len = ParsedRequest_totalLen(parsedRequest);
    char *new_request = (char *)malloc(total_len);
    if (ParsedRequest_unparse(parsedRequest, new_request, total_len) < 0) {
        char *response = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
        write(client_fd, response, strlen(response));
        free(new_request);
        ParsedRequest_destroy(parsedRequest);
        return;
    }
    struct hostent *server = gethostbyname(host);

    //modify--end
    char request[RECV_BUFFER_SIZE];
    snprintf(request, sizeof(request), "%s %s HTTP/1.0\r\nHost: %s:%s\r\nConnection: close\r\n\r\n",
         parsedRequest->method, path, host, port);

    printf("Creating socket to connect to %s:%s\n", host, port);
    // Create a socket to connect to the remote server
    int remote_fd;
    struct addrinfo *res;
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if(getaddrinfo(host, port, &hints, &res) != 0){
        fprintf(stderr, "Failed to get server address\n");
        close(client_fd);
        close(remote_fd);
        return;
    }

    remote_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (remote_fd == -1) {
        perror("socket");
        close(client_fd);
        return;
    }
    if(connect(remote_fd, res->ai_addr, res->ai_addrlen) == -1){
        perror("connect");
        close(client_fd);
        close(remote_fd);
        return;
    }
    // Send the request to the remote server
    printf("Sending request to remote server:\n%s\n", request);
    //modify--start
    if(send(remote_fd, new_request, strlen(new_request), 0) == -1){
        perror("send");
        close(client_fd);
        close(remote_fd);
        return;
    }
    //modify--end
    // if(send(remote_fd, request, strlen(request), 0) == -1){
    //     perror("send");
    //     close(client_fd);
    //     close(remote_fd);
    //     return;
    // }
    ssize_t bytesSent;
    while((bytesRead = recv(remote_fd, buffer, sizeof(buffer), 0)) > 0){
        bytesSent = send(client_fd, buffer, bytesRead, 0);
        if(bytesSent == -1){
            perror("send");
            close(client_fd);
            close(remote_fd);
            return;
        }
    }
    if(bytesRead == -1){
        perror("recv");
        close(client_fd);
        close(remote_fd);
        return;
    }
    close(client_fd);
    ParsedRequest_destroy(parsedRequest);
    close(remote_fd);

    
}

int proxy(char *proxy_port) {
    int server_socket, client_socket;
    socklen_t addr_size;
    int status;
    struct addrinfo hints, *res, *p;
    struct sockaddr_storage client_addr;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, proxy_port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server:socket");
            continue;
        }

        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("server:bind");
            continue;
        }

        break;
    }
    freeaddrinfo(res); // all done with this structure

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(server_socket, QUEUE_LENGTH) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while (1) {
        // Accept incoming connection
        addr_size = sizeof client_addr;
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

        if (client_socket == -1) {
            perror("accept"); 
            continue;
        }

        // Fork a new process to handle the client request
        if (fork() == 0) {
            close(server_socket); // Close in child process
            printf("Handling client request in child process\n");

            handle_client(client_socket);
            printf("Finished handling client request in child process\n");

            close(client_socket); // Close in child process before exit
            exit(0);
        }

        close(client_socket); // Close in parent process
    }

    return 0;
}

int main(int argc, char *argv[]) {
    char *proxy_port;

    if (argc != 2) {
        fprintf(stderr, "Usage: ./proxy <port>\n");
        exit(EXIT_FAILURE);
    }

    proxy_port = argv[1];
    return proxy(proxy_port);
}
