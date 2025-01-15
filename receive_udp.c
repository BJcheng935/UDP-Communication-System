#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 65536 // 2^16 as per requirements
#define IP_BUFFER_SIZE 64

static int convert_port_name(uint16_t *port, const char *port_name) { // Function to convert port name to port number
    char *end;
    long long int nn;
    uint16_t t;
    long long int tt;

    if (port_name == NULL) return -1; // Check if port_name is NULL
    if (*port_name == '\0') return -1; // Check if port_name is empty
    
    nn = strtoll(port_name, &end, 0);
    if (*end != '\0') return -1; // Check if port_name contains invalid characters
    if (nn < ((long long int) 0)) return -1; // Check if port_name is negative
    
    t = (uint16_t) nn; // Convert port_name to port number
    tt = (long long int) t; // Convert port number to long long int
    if (tt != nn) return -1; // Check if port number is not an integer
    
    *port = t;
    return 0;
}

void print_client_info(struct sockaddr_in *client_addr) { // Function to print client information
    char ip_str[IP_BUFFER_SIZE]; 
    DWORD ip_str_len = IP_BUFFER_SIZE;
    
    inet_ntop(AF_INET, &(client_addr->sin_addr), ip_str, ip_str_len); // Convert IP address to string
    printf("[Client %s:%d] ", ip_str, ntohs(client_addr->sin_port)); // Print client information
}

int main(int argc, char *argv[]) { // Main function
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { // Initialize Winsock
        fprintf(stderr, "WSAStartup failed\n"); // Print error message
        return 1;
    }

    if (argc < 2) { // Check if port name is provided
        fprintf(stderr, "Usage: %s <port_name>\n", argv[0]); // Print usage message
        WSACleanup();
        return 1;
    }

    uint16_t port;
    if (convert_port_name(&port, argv[1]) != 0) { // Convert port name to port number
        fprintf(stderr, "Invalid port name: %s\n", argv[1]); // Print error message
        WSACleanup();
        return 1;
    }

    struct addrinfo hints; // Set up UDP socket
    struct addrinfo *result, *rp; 
    SOCKET sfd = INVALID_SOCKET;

    memset(&hints, 0, sizeof(hints)); // Initialize hints
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    char port_str[6]; // Convert port number to string
    snprintf(port_str, sizeof(port_str), "%u", port);

    int s = getaddrinfo(NULL, port_str, &hints, &result); // Get address information
    if (s != 0) { // Check if getaddrinfo returns an error
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        WSACleanup();
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) { // Loop through address information
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == INVALID_SOCKET) // Check if socket creation fails
            continue;

        if (bind(sfd, rp->ai_addr, (int)rp->ai_addrlen) == 0) // Bind socket
            break;

        closesocket(sfd);
    }

    if (rp == NULL) { // Check if socket creation or binding fails
        fprintf(stderr, "Could not bind\n"); // Print error message
        freeaddrinfo(result);
        WSACleanup(); // Cleanup Winsock
        return 1;
    }

    freeaddrinfo(result); // Free address information

    printf("UDP Echo Server listening on port %u...\n", port);
    printf("Ready to receive and echo messages.\n");
    printf("---------------------------------------\n");

    char buffer[BUFFER_SIZE];
    int bytes_read;
    struct sockaddr_in peer_addr;
    int peer_addr_len = sizeof(peer_addr);
    unsigned long msg_count = 0;

    while (1) { // Loop to receive and echo messages
        bytes_read = recvfrom(sfd, buffer, BUFFER_SIZE, 0,(struct sockaddr *)&peer_addr, &peer_addr_len);

        if (bytes_read == SOCKET_ERROR) { // Check if receive fails
            fprintf(stderr, "Error receiving data: %d\n", WSAGetLastError());
            break;
        }

        msg_count++;
        
        buffer[bytes_read] = '\0';// Ensure null termination for printing

        print_client_info(&peer_addr);// Print message info
        printf("Received message #%lu (%d bytes): ", msg_count, bytes_read);
        
        if (bytes_read == 0) { // Check if message is empty
            printf("<empty packet>\n");
        } else if (bytes_read <= 50) { // Check if message is less than 50 bytes
            printf("%s\n", buffer);
        } else {
            printf("%.50s... (message truncated)\n", buffer);
        }

        // Echo data back
        if (sendto(sfd, buffer, bytes_read, 0,
                  (struct sockaddr *)&peer_addr, peer_addr_len) == SOCKET_ERROR) {
            fprintf(stderr, "Error sending response: %d\n", WSAGetLastError()); // Check if send fails
            break;
        }

        printf("Message echoed back successfully\n");
    }

    closesocket(sfd); // Close socket
    WSACleanup(); // Cleanup Winsock
    return 0; // Return success
}