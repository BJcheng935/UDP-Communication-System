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

    if (port_name == NULL || *port_name == '\0') // Check if port_name is NULL
        return -1;

    nn = strtoll(port_name, &end, 0);
    if (*end != '\0')// Check if port_name contains invalid characters
        return -1;
    if (nn < 0) // Check if port_name is negative
        return -1;

    t = (uint16_t) nn;
    tt = (long long int) t;
    if (tt != nn) // Check if port number is not an integer
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
        fprintf(stderr, "Usage: %s <tcp_port> <udp_server> <udp_port>\n", argv[0]);
        WSACleanup();
        return 1;
    }

    // Parse TCP port
    uint16_t tcp_port;
    if (convert_port_name(&tcp_port, argv[1]) != 0) {
        fprintf(stderr, "Invalid TCP port: %s\n", argv[1]);
        WSACleanup();
        return 1;
    }

    char *udp_server = argv[2];
    char *udp_port = argv[3];

    // Create TCP listening socket
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Create TCP socket
    if (listen_socket == INVALID_SOCKET) { // Check if socket creation was successful
        fprintf(stderr, "TCP socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Bind TCP socket
    struct sockaddr_in tcp_addr;
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr.sin_port = htons(tcp_port);

    if (bind(listen_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "TCP bind failed: %d\n", WSAGetLastError()); // Check if binding was successful
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    // Listen for connections
    if (listen(listen_socket, 1) == SOCKET_ERROR) {
        fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    printf("Tunnel server listening on TCP port %d...\n", tcp_port);

    // Create UDP socket and resolve UDP server address
    struct addrinfo hints, *result, *rp;
    SOCKET udp_socket = INVALID_SOCKET;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(udp_server, udp_port, &hints, &result) != 0) { // Resolve UDP server address
        fprintf(stderr, "getaddrinfo failed for UDP server: %d\n", WSAGetLastError()); // Check if resolution was successful
        closesocket(listen_socket); // Close TCP socket
        WSACleanup(); // Deinitialize Winsock
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) { // Connect to UDP server
        udp_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (udp_socket == INVALID_SOCKET) // Check if socket creation was successful
            continue;

        if (connect(udp_socket, rp->ai_addr, (int)rp->ai_addrlen) != SOCKET_ERROR) // Connect to UDP server
            break;

        closesocket(udp_socket); // Close UDP socket
    }

    freeaddrinfo(result);// Free memory

    if (rp == NULL) { // Check if connection was successful
        fprintf(stderr, "Could not connect to UDP server\n");
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    printf("Connected to UDP server %s:%s\n", udp_server, udp_port);

    SOCKET client_socket = accept(listen_socket, NULL, NULL);// Accept TCP connection
    if (client_socket == INVALID_SOCKET) { // Check if connection was successful
        fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
        closesocket(udp_socket);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    printf("Accepted TCP connection. Starting tunnel...\n");

    // Buffers for data handling
    char udp_buffer[UDP_BUFFER_SIZE];
    char tcp_buffer[TCP_BUFFER_SIZE];
    char reconstruction_buffer[RECONSTRUCTION_BUFFER_SIZE];
    int reconstruction_index = 0;

    fd_set readfds;

    while (1) { // Loop until client disconnects
        FD_ZERO(&readfds);
        FD_SET(client_socket, &readfds);
        FD_SET(udp_socket, &readfds);

        if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) { // Check if select was successful
            fprintf(stderr, "select failed: %d\n", WSAGetLastError());
            break;
        }

        // Handle TCP data
        if (FD_ISSET(client_socket, &readfds)) { // Check if TCP data is available
            int bytes_read = recv(client_socket, tcp_buffer, TCP_BUFFER_SIZE, 0);
            if (bytes_read == SOCKET_ERROR) { // Check if TCP data was received
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
            while (reconstruction_index - processed >= 2) {
                uint16_t msg_length = ((uint16_t)(reconstruction_buffer[processed]) << 8) |
                                    (uint8_t)(reconstruction_buffer[processed + 1]);

                if (reconstruction_index - processed < msg_length + 2) // Check if message is complete
                    break;

                if (send(udp_socket, reconstruction_buffer + processed + 2, msg_length, 0) == SOCKET_ERROR) {
                    fprintf(stderr, "UDP send failed: %d\n", WSAGetLastError());
                    goto cleanup;
                }

                processed += msg_length + 2; // Move to next message
            }

            if (processed > 0) {//Move any remaining data to the start of the buffer
                memmove(reconstruction_buffer, reconstruction_buffer + processed,
                       reconstruction_index - processed); // Move data to the start of the buffer
                reconstruction_index -= processed;
            }
        }

        // Handle UDP data
        if (FD_ISSET(udp_socket, &readfds)) {
            int bytes_read = recv(udp_socket, udp_buffer, UDP_BUFFER_SIZE, 0);
            if (bytes_read == SOCKET_ERROR) { // Check if UDP data was received
                fprintf(stderr, "UDP receive failed: %d\n", WSAGetLastError());
                break;
            }

            // Prepare TCP message: length + data
            tcp_buffer[0] = (bytes_read >> 8) & 0xFF;
            tcp_buffer[1] = bytes_read & 0xFF;
            memcpy(tcp_buffer + 2, udp_buffer, bytes_read);

            if (send(client_socket, tcp_buffer, bytes_read + 2, 0) == SOCKET_ERROR) { // Send TCP message
                fprintf(stderr, "TCP send failed: %d\n", WSAGetLastError());
                break;
            }
        }
    }

cleanup:
    closesocket(client_socket); // Close TCP socket
    closesocket(udp_socket);// Close UDP socket
    closesocket(listen_socket);// Close TCP socket
    WSACleanup();// Cleanup Winsock
    return 0;
}