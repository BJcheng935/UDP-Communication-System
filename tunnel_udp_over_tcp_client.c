#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>

#pragma comment(lib, "ws2_32.lib")

#define UDP_BUFFER_SIZE 65536  // 2^16
#define TCP_BUFFER_SIZE 65538  // UDP_BUFFER_SIZE + 2 bytes for length
#define RECONSTRUCTION_BUFFER_SIZE 131076  // 2^17 + 4

static int convert_port_name(uint16_t *port, const char *port_name) {
    char *end;
    long long int nn;
    uint16_t t;
    long long int tt;

    if (port_name == NULL || *port_name == '\0')
        return -1;

    nn = strtoll(port_name, &end, 0);
    if (*end != '\0')
        return -1;
    if (nn < 0)
        return -1;

    t = (uint16_t) nn;
    tt = (long long int) t;
    if (tt != nn)
        return -1;

    *port = t;
    return 0;
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { // Initialize Winsock
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    if (argc < 4) { // Check if port name is provided
        fprintf(stderr, "Usage: %s <udp_port> <tcp_server> <tcp_port>\n", argv[0]);
        WSACleanup();
        return 1;
    }

    uint16_t udp_port;// Parse UDP port
    if (convert_port_name(&udp_port, argv[1]) != 0) { // Convert port name to port number
        fprintf(stderr, "Invalid UDP port: %s\n", argv[1]);
        WSACleanup();
        return 1;
    }

    char *tcp_server = argv[2];
    char *tcp_port = argv[3];
    
    SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);// Create UDP socket
    if (udp_socket == INVALID_SOCKET) { // Check if socket creation was successful
        fprintf(stderr, "UDP socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    
    struct sockaddr_in udp_addr;// Bind UDP socket
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_addr.sin_port = htons(udp_port);

    if (bind(udp_socket, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) == SOCKET_ERROR) { 
        fprintf(stderr, "UDP bind failed: %d\n", WSAGetLastError()); // Check if binding was successful
        closesocket(udp_socket); // Close socket
        WSACleanup();
        return 1;
    }

    struct addrinfo hints, *result, *rp;// Create TCP socket and connect to server
    SOCKET tcp_socket = INVALID_SOCKET; // Initialize socket variable

    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(tcp_server, tcp_port, &hints, &result) != 0) { // Convert port name to port number
        fprintf(stderr, "getaddrinfo failed: %d\n", WSAGetLastError()); // Check if getaddrinfo was successful
        closesocket(udp_socket); // Close socket
        WSACleanup();
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) { // Connect to server
        tcp_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (tcp_socket == INVALID_SOCKET) // Check if socket creation was successful
            continue;

        if (connect(tcp_socket, rp->ai_addr, (int)rp->ai_addrlen) != SOCKET_ERROR) // Check if connection was successful
            break;

        closesocket(tcp_socket); // Close socket
    }

    freeaddrinfo(result); // Free address information

    if (rp == NULL) { // Check if connection was successful
        fprintf(stderr, "Could not connect to TCP server\n");
        closesocket(udp_socket); // Close socket
        WSACleanup(); // Cleanup Winsock
        return 1;
    }

    char udp_buffer[UDP_BUFFER_SIZE];// Buffers for data handling
    char tcp_buffer[TCP_BUFFER_SIZE];
    char reconstruction_buffer[RECONSTRUCTION_BUFFER_SIZE];
    int reconstruction_index = 0;

    struct sockaddr_in peer_addr;
    int peer_addr_len = sizeof(peer_addr);
    int have_peer_addr = 0;

    fd_set readfds;
    
    printf("Tunnel client ready. Listening on UDP port %d and connected to TCP server %s:%s\n", 
           udp_port, tcp_server, tcp_port); // Print ready message

    while (1) { // Loop until client disconnects
        FD_ZERO(&readfds);
        FD_SET(udp_socket, &readfds);
        FD_SET(tcp_socket, &readfds);

        if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) { // Check if select was successful
            fprintf(stderr, "select failed: %d\n", WSAGetLastError());
            break;
        }

        if (FD_ISSET(udp_socket, &readfds)) {// Handle UDP data
            int bytes_read = recvfrom(udp_socket, udp_buffer, UDP_BUFFER_SIZE, 0,
                                    (struct sockaddr*)&peer_addr, &peer_addr_len);
            if (bytes_read == SOCKET_ERROR) { // Check if receive was successful
                fprintf(stderr, "UDP receive failed: %d\n", WSAGetLastError());
                break;
            }

            have_peer_addr = 1;

            // Prepare TCP message: length + data
            uint16_t length = (uint16_t)bytes_read;
            tcp_buffer[0] = (length >> 8) & 0xFF;
            tcp_buffer[1] = length & 0xFF;
            memcpy(tcp_buffer + 2, udp_buffer, bytes_read);

            // Send to TCP server
            if (send(tcp_socket, tcp_buffer, bytes_read + 2, 0) == SOCKET_ERROR) {
                fprintf(stderr, "TCP send failed: %d\n", WSAGetLastError());
                break;
            }
        }

        // Handle TCP data
        if (FD_ISSET(tcp_socket, &readfds)) {
            int bytes_read = recv(tcp_socket, tcp_buffer, TCP_BUFFER_SIZE, 0);
            if (bytes_read == SOCKET_ERROR) {
                fprintf(stderr, "TCP receive failed: %d\n", WSAGetLastError());
                break;
            }
            if (bytes_read == 0) {  // TCP connection closed
                break;
            }

            // Copy new data to reconstruction buffer
            memcpy(reconstruction_buffer + reconstruction_index, tcp_buffer, bytes_read);
            reconstruction_index += bytes_read;

            // Process complete messages
            int processed = 0;
            while (reconstruction_index - processed >= 2) { // Check if message is complete
                uint16_t msg_length = ((uint16_t)(reconstruction_buffer[processed]) << 8) |
                                    (uint8_t)(reconstruction_buffer[processed + 1]);

                if (reconstruction_index - processed < msg_length + 2)
                    break;

                if (have_peer_addr) { // Send to UDP server
                    if (sendto(udp_socket, reconstruction_buffer + processed + 2, msg_length, 0,
                             (struct sockaddr*)&peer_addr, peer_addr_len) == SOCKET_ERROR) {
                        fprintf(stderr, "UDP send failed: %d\n", WSAGetLastError());
                        goto cleanup;
                    }
                }

                processed += msg_length + 2;
            }

            // Move any remaining data to the start of the buffer
            if (processed > 0) {
                memmove(reconstruction_buffer, reconstruction_buffer + processed,
                       reconstruction_index - processed);
                reconstruction_index -= processed;
            }
        }
    }

cleanup: 
    closesocket(udp_socket);// Close socket
    closesocket(tcp_socket);
    WSACleanup();// Cleanup Winsock
    return 0;
}