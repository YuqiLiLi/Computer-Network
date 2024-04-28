#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#define QUEUE_LENGTH 10
#define RECV_BUFFER_SIZE 2048

/* TODO: server()
 * Open socket and wait for client to connect
 * Print received message to stdout
 * Return 0 on success, non-zero on failure
*/
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int server(char *server_port) {
    int server_socket, client_socket;
    socklen_t addr_size;
    int status;
    struct addrinfo hints, *res, *p;
    struct sockaddr_storage client_addr;
    char buffer[RECV_BUFFER_SIZE];
    int yes=1;
    // struct sigaction sa;
    

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 

    if ((status=getaddrinfo(NULL, server_port, &hints, &res))!= 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server:socket");
            continue;
        }

    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
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

  // sa.sa_handler =sigchld_handler;
  // sigemptyset(&sa.sa_mask);
  //   sa.sa_flags = SA_RESTART;
  //   if (sigaction(SIGCHLD, &sa, NULL) == -1) {
  //       perror("sigaction");
  //       exit(1);
  //   }

  printf("server: waiting for connections...\n");

    while (1) {
        addr_size = sizeof client_addr;
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size)) == -1) {
            perror("error");
            close(server_socket);
            exit(1);
        }

        printf("success connected\n");

        ssize_t bytes_received;
        while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
            // buffer[bytes_received] = '\0';
            // fprintf(stdout, "Received message: %s\n", buffer);
            write(1, buffer, bytes_received);
        }

        if (bytes_received == 0) {
            printf("Client disconnected.\n");
        } else if (bytes_received == -1) {
            perror("Error receiving data from client");
        }

        close(client_socket);

        printf("server: waiting for connections...\n");
    }

    close(server_socket);

    return 0;
}

/*
 * main():
 * Parse command-line arguments and call server function
*/
int main(int argc, char **argv) {
  char *server_port;

  if (argc != 2) {
    fprintf(stderr, "Usage: ./server-c [server port]\n");
    exit(EXIT_FAILURE);
  }

  server_port = argv[1];
  return server(server_port);

  
}
