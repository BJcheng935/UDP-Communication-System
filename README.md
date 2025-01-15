# UDP-Communication-System

This repository contains a collection of Windows-based network communication programs implementing various UDP (User Datagram Protocol) functionalities, including basic UDP communication, UDP-over-TCP tunneling, and UDP echo services.

## Components

### 1. Basic UDP Communication
- **send_udp.c**: A UDP client that sends data from stdin to a UDP server
- **receive_udp.c**: A UDP server that receives and echoes back UDP datagrams
- **send_receive_udp.c**: A bidirectional UDP client that can simultaneously send and receive data
- **reply_udp.c**: A simple UDP echo server

### 2. UDP-over-TCP Tunneling
- **tunnel_udp_over_tcp_client.c**: Client component of the UDP-over-TCP tunnel
- **tunnel_udp_over_tcp_server.c**: Server component of the UDP-over-TCP tunnel

## Features

- Binary-safe data handling
- Support for large datagrams (up to 64KB)
- Proper error handling and resource cleanup
- Non-blocking I/O for simultaneous send/receive operations
- TCP tunneling for UDP traffic

## Buffer Sizes

- UDP Buffer: 65536 bytes (2^16)
- TCP Buffer: 65538 bytes (UDP buffer + 2 bytes for length)
- Reconstruction Buffer: 131076 bytes (2^17 + 4)

## Building

Compile each program using a C compiler with Windows Sockets support. Example using Microsoft Visual C++:

```batch
cl program_name.c /link ws2_32.lib
```

## Usage

### Basic UDP Echo Server
```bash
receive_udp.c <port>
reply_udp.c <port>
```

### UDP Client
```bash
send_udp.c <server_name> <port>
```

### Bidirectional UDP Client
```bash
send_receive_udp.c <server_name> <port>
```

### UDP-over-TCP Tunnel
```bash
# Start the tunnel server
tunnel_udp_over_tcp_server.c <tcp_port> <udp_server> <udp_port>

# Start the tunnel client
tunnel_udp_over_tcp_client.c <udp_port> <tcp_server> <tcp_port>
```

## Implementation Details

### Port Name Conversion
All programs use a robust `convert_port_name` function that:
- Validates port number input
- Handles numeric overflow
- Ensures port numbers are within valid range

### UDP Echo Server Features
- Displays client IP address and port
- Shows message size and content
- Truncates displayed messages over 50 bytes
- Maintains message count
- Handles empty packets

### Send/Receive UDP Features
- Binary mode support for stdin/stdout
- Non-blocking I/O using select()
- Proper EOF handling
- Buffer management for partial reads/writes

### UDP-over-TCP Tunnel Features
- Message framing using length prefixes
- Buffer reconstruction for split TCP messages
- Maintains UDP datagram boundaries
- Handles multiple UDP endpoints
- Efficient buffer management

## Error Handling

The programs include comprehensive error handling for:
- Socket operations
- Memory operations
- Network connections
- Data transmission
- Resource allocation/deallocation

## Notes

1. All programs use Windows Sockets 2 (Winsock2) API
2. Programs require administrative privileges for binding to ports below 1024
3. The UDP-over-TCP tunnel maintains datagram boundaries while tunneling through TCP
4. Buffer sizes are optimized for typical UDP datagram sizes
5. All programs properly cleanup resources on exit

## Dependencies

- Windows OS
- Winsock2 library (ws2_32.lib)
- C Runtime Library

## Best Practices Implemented

1. **Resource Management**
   - Proper socket cleanup
   - Memory management
   - Handle closing

2. **Error Handling**
   - Comprehensive error checking
   - Informative error messages
   - Graceful failure handling

3. **Buffer Management**
   - Proper buffer sizing
   - Overflow prevention
   - Efficient data copying

4. **Network Programming**
   - Non-blocking I/O
   - Proper protocol handling
   - Connection management

## Security Considerations

1. Input validation for all command-line arguments
2. Buffer size checks to prevent overflow
3. Proper handling of network data
4. Resource cleanup in error cases
5. No hardcoded sensitive information

## Limitations

1. Windows-specific implementation
2. No encryption/authentication
3. Single-client tunnel server
4. Fixed buffer sizes
5. No configuration file support