#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "ws2_32.lib")

#define INPUT_BUFFER_SIZE 480
#define RECEIVE_BUFFER_SIZE 65536  // 2^16 as per requirements

int better_read(HANDLE fd, char *buf, size_t count) {// Better read implementation for handling partial reads
    DWORD bytes_read;
    if (ReadFile(fd, buf, (DWORD)count, &bytes_read, NULL) == 0) { // Read data from file
        return -1;
    }
    return bytes_read;
}

int better_write(HANDLE fd, const char *buf, size_t count) {// Better write implementation for handling partial writes
    DWORD bytes_written;
    if (WriteFile(fd, buf, (DWORD)count, &bytes_written, NULL) == 0) { // Write data to file
        return -1;
    }
    return bytes_written; // Return number of bytes written
} 

int main(int argc, char *argv[]) { 
    WSADATA wsaData;// Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { 
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
    
    struct addrinfo hints;// Set up UDP socket
    struct addrinfo *result, *rp;
    SOCKET sfd = INVALID_SOCKET;

    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = 0;

    int s = getaddrinfo(server_name, port_name, &hints, &result);
    if (s != 0) { // Convert port name to port number
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        WSACleanup(); // Cleanup Winsock
        return 1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) { // Connect to server
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == INVALID_SOCKET) // Create socket
            continue;

        if (connect(sfd, rp->ai_addr, (int)rp->ai_addrlen) != SOCKET_ERROR) 
            break;

        closesocket(sfd); // Close socket
    }

    if (rp == NULL) { // Check if connection was successful
        fprintf(stderr, "Could not connect\n");
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result); // Free address information

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);// Get standard input handle
    if (hStdin == INVALID_HANDLE_VALUE) { // Check if handle is valid
        fprintf(stderr, "Could not get stdin handle\n");
        closesocket(sfd);
        WSACleanup();
        return 1;
    }

    // Set stdin to binary mode
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        fprintf(stderr, "Could not set binary mode for stdin\n");
        closesocket(sfd);
        WSACleanup();
        return 1;
    }

    // Set stdout to binary mode
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "Could not set binary mode for stdout\n");
        closesocket(sfd);
        WSACleanup();
        return 1;
    }

    char input_buffer[INPUT_BUFFER_SIZE];
    char receive_buffer[RECEIVE_BUFFER_SIZE];
    fd_set readfds;
    HANDLE handles[1] = { hStdin };
    int stdin_eof = 0;
    
    printf("Connected to UDP server. Ready to send/receive messages.\n");
    printf("Type your messages and press Enter to send.\n");

    while (!stdin_eof) { // Loop until EOF is detected
        FD_ZERO(&readfds);
        FD_SET(sfd, &readfds);

        DWORD console_result = WaitForMultipleObjects(1, handles, FALSE, 0);// Check for console input
        if (console_result == WAIT_OBJECT_0) {
            int bytes_read = better_read(hStdin, input_buffer, INPUT_BUFFER_SIZE);
            if (bytes_read > 0) {// Null terminate the input buffer for proper string handling
                input_buffer[bytes_read] = '\0';
                
                if (send(sfd, input_buffer, bytes_read, 0) == SOCKET_ERROR) {// Send the message
                    fprintf(stderr, "Error sending data: %d\n", WSAGetLastError());
                    break;
                }
                
                printf("Sent %d bytes\n", bytes_read);
            } else if (bytes_read == 0) { // Check if EOF is detected
                stdin_eof = 1;
                printf("EOF detected, sending empty packet...\n");
                if (send(sfd, "", 0, 0) == SOCKET_ERROR) { // Send the EOF packet
                    fprintf(stderr, "Error sending EOF packet: %d\n", WSAGetLastError());
                    break;
                }
            } else { // Error reading from stdin
                fprintf(stderr, "Error reading from stdin\n");
                break;
            }
        }

        struct timeval tv;// Check for socket data
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout

        if (select(0, &readfds, NULL, NULL, &tv) == SOCKET_ERROR) { // Check for socket data
            fprintf(stderr, "select error: %d\n", WSAGetLastError());
            break;
        }

        if (FD_ISSET(sfd, &readfds)) { // Check if data is available
            int bytes_read = recv(sfd, receive_buffer, RECEIVE_BUFFER_SIZE, 0);
            if (bytes_read == SOCKET_ERROR) { // Error receiving data
                fprintf(stderr, "Error receiving data: %d\n", WSAGetLastError());
                break;
            }

            if (bytes_read == 0) { // Check if EOF is detected
                printf("Received empty packet, exiting...\n");
                break;
            }

            receive_buffer[bytes_read] = '\0';// Null terminate the receive buffer
            
            printf("Received %d bytes: ", bytes_read); // Print received data
            HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
            if (better_write(hStdout, receive_buffer, bytes_read) != bytes_read) { // Write received data to stdout
                fprintf(stderr, "Error writing to stdout\n");
                break;
            }
            printf("\n");  // Add newline after each received message
        }

        Sleep(10);  // Small delay to prevent tight loop
    }

    closesocket(sfd); // Close socket
    WSACleanup(); // Free Winsock resources
    return 0; // Return success
}