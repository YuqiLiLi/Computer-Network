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

#define SEND_BUFFER_SIZE 2048


/* TODO: client()
 * Open socket and send message from stdin.
 * Return 0 on success, non-zero on failure
*/
int client(char *server_ip, char *server_port) {
    int client_socket;
    struct addrinfo hints, *res, *p;
    char send_buffer[SEND_BUFFER_SIZE];

    // Set up hints for getaddrinfo
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    // Get address information
    if (getaddrinfo(server_ip, server_port, &hints, &res) != 0) {
        perror("Error getting address information");
    }

    // Iterate through the results and connect to the first valid one
    for (p = res; p != NULL; p = p->ai_next) {
        if ((client_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(client_socket, p->ai_addr, p->ai_addrlen) == 0) {
            break; // Successfully connected
        }

        close(client_socket);
    }

    freeaddrinfo(res); // Free the linked list

    if (p == NULL) {
        perror("Error connecting to server");
        return 1;
    }
   

    
    // Read from stdin and send messages to the server
    ssize_t bytes_read;
    while ((bytes_read = read(STDIN_FILENO, send_buffer, sizeof(send_buffer))) > 0) {
        if (send(client_socket, send_buffer, bytes_read, 0) == -1) {
            perror("Error sending data to server");
            close(client_socket);
            return 1;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading from stdin");
        close(client_socket);
        return 1;
    }

    // Close the client socket
    close(client_socket);

    return 0;
}

/*
 * main()
 * Parse command-line arguments and call client function
*/
int main(int argc, char **argv) {
  char *server_ip;
  char *server_port;

  if (argc != 3) {
    fprintf(stderr, "Usage: ./client-c [server IP] [server port] < [message]\n");
    exit(EXIT_FAILURE);
  }

  server_ip = argv[1];
  server_port = argv[2];
  return client(server_ip, server_port);
}
