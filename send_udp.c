#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 480 // As per requirements

// Better read implementation for handling partial reads
int better_read(FILE* fd, char *buf, size_t count) {
    size_t bytes_read = fread(buf, 1, count, fd);
    if (bytes_read == 0 && ferror(fd)) {
        return -1;
    }
    return bytes_read;
}

int main(int argc, char *argv[]) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { // Initialize Winsock
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    if (argc < 3) { // Check if port name is provided
        fprintf(stderr, "Usage: %s <server_name> <port_name>\n", argv[0]);
        WSACleanup();
        return 1;
    }

    char *server_name = argv[1];
    char *port_name = argv[2];

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    SOCKET sfd = INVALID_SOCKET;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = 0;

    int s = getaddrinfo(server_name, port_name, &hints, &result);
    if (s != 0) { // Convert port name to port number
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s)); // Print error message
        WSACleanup();
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) { // Set up UDP socket
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); // Create socket
        if (sfd == INVALID_SOCKET) // Check if socket was created
            continue;

        if (connect(sfd, rp->ai_addr, (int)rp->ai_addrlen) != SOCKET_ERROR) // Connect to server
            break;

        closesocket(sfd);
    }

    if (rp == NULL) { // Check if socket was created
        fprintf(stderr, "Could not connect\n");
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    char buffer[BUFFER_SIZE];
    int bytes_read;

    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {// Set stdin to binary mode
        fprintf(stderr, "Could not set binary mode\n");
        closesocket(sfd);
        WSACleanup();
        return 1;
    }

    while ((bytes_read = better_read(stdin, buffer, BUFFER_SIZE)) > 0) { // Read from stdin
        if (send(sfd, buffer, bytes_read, 0) == SOCKET_ERROR) { // Send data
            fprintf(stderr, "Error sending data: %d\n", WSAGetLastError());
            closesocket(sfd);
            WSACleanup();
            return 1;
        }
    }

    if (bytes_read == -1) { // Check if read was successful
        fprintf(stderr, "Error reading from stdin\n");
        closesocket(sfd);
        WSACleanup();
        return 1;
    }

    if (send(sfd, "", 0, 0) == SOCKET_ERROR) {// Send empty packet to signal EOF
        fprintf(stderr, "Error sending EOF packet: %d\n", WSAGetLastError()); // Print error message
        closesocket(sfd);
        WSACleanup();
        return 1;
    }

    closesocket(sfd);// Close socket
    WSACleanup();// Cleanup Winsock
    return 0;// Return success
}