#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>

#pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib

#define BUFFER_SIZE 65536 // 2^16 as per requirements

static int convert_port_name(uint16_t *port, const char *port_name) { // Function to convert port name to port number
    char *end;
    long long int nn;
    uint16_t t;
    long long int tt;

    if (port_name == NULL) return -1;
    if (*port_name == '\0') return -1;
    
    nn = strtoll(port_name, &end, 0);
    if (*end != '\0') return -1;
    if (nn < ((long long int) 0)) return -1;
    
    t = (uint16_t) nn;
    tt = (long long int) t;
    if (tt != nn) return -1;
    
    *port = t;
    return 0;
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;// Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { // Initialize Winsock
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    if (argc < 2) { // Check if port name is provided
        fprintf(stderr, "Usage: %s <port_name>\n", argv[0]);
        WSACleanup();
        return 1;
    }

    uint16_t port; // Set up UDP socket
    if (convert_port_name(&port, argv[1]) != 0) { // Convert port name to port number
        fprintf(stderr, "Invalid port name: %s\n", argv[1]);
        WSACleanup();
        return 1;
    }

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    SOCKET sfd = INVALID_SOCKET;

    memset(&hints, 0, sizeof(hints)); // Initialize hints
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    char port_str[6]; // Convert port number to string
    snprintf(port_str, sizeof(port_str), "%u", port); // Convert port number to string

    int s = getaddrinfo(NULL, port_str, &hints, &result);
    if (s != 0) { // Check if getaddrinfo failed
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        WSACleanup();
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) { // Loop through results
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == INVALID_SOCKET) // Check if socket creation failed
            continue;

        if (bind(sfd, rp->ai_addr, (int)rp->ai_addrlen) == 0) // Bind socket
            break;

        closesocket(sfd);
    }

    if (rp == NULL) { // Check if socket creation or binding failed
        fprintf(stderr, "Could not bind\n");
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    char buffer[BUFFER_SIZE];
    int bytes_read;
    struct sockaddr_storage peer_addr;
    int peer_addr_len;

    printf("UDP Echo Server listening on port %u...\n", port);

    while (1) { // Loop forever
        peer_addr_len = sizeof(peer_addr); // Set peer address length
        bytes_read = recvfrom(sfd, buffer, BUFFER_SIZE, 0, 
                            (struct sockaddr *)&peer_addr, &peer_addr_len); // Receive data
        
        if (bytes_read == SOCKET_ERROR) { // Check if receive failed
            fprintf(stderr, "Error receiving data: %d\n", WSAGetLastError());
            break;
        }

        // Echo data back to sender
        if (sendto(sfd, buffer, bytes_read, 0,
                  (struct sockaddr *)&peer_addr, peer_addr_len) == SOCKET_ERROR) {
            fprintf(stderr, "Error sending response: %d\n", WSAGetLastError());
            break;
        }
    }

    closesocket(sfd);
    WSACleanup();
    return 0;
}