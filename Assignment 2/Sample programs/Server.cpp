// Server.cpp

#define _WINSOCK_DEPRECATED_NO_WARNINGS // Disable deprecated API warnings
#define _CRT_SECURE_NO_WARNINGS         // Disable secure function warnings

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib") // Link the library of "ws2_32.lib"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 50000

int main(int argc, char **argv) {

    char szBuff[100];
    int msg_len;
    int addr_len;
    struct sockaddr_in local, client_addr;

    SOCKET sock, msg_sock;
    WSADATA wsaData;

    // if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) delete at Feb 14, 2025
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        // stderr: standard error are printed to the screen.
        fprintf(stderr, "WSAStartup failed with error %d\n", WSAGetLastError());
        // WSACleanup function terminates use of the Windows Sockets DLL.
        WSACleanup();
        return -1;
    } // end if

    // Fill in the address structure
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(DEFAULT_PORT);

    sock = socket(AF_INET, SOCK_STREAM, 0); // TCp socket

    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed with error %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    } // end if

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed with error %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    } // end if

    // waiting for connections
    if (listen(sock, 5) == SOCKET_ERROR) {
        fprintf(stderr, "listen() failed with error %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    } // end if

    printf("Waiting for connections ........\n");

    addr_len = sizeof(client_addr);
    msg_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
    if (msg_sock == INVALID_SOCKET) {
        fprintf(stderr, "accept() failed with error %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    } // end if

    printf("accepted connection from %s, port %d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    while (1) {
        msg_len = recv(msg_sock, szBuff, sizeof(szBuff) - 1, 0); // newly modified on Feb 14, 2025

        if (msg_len == SOCKET_ERROR) {
            fprintf(stderr, "recv() failed with error %d\n", WSAGetLastError());
            WSACleanup();
            return -1;
        } // end if

        if (msg_len == 0) {
            printf("Client closed connection\n");
            closesocket(msg_sock);
            return -1;
        } // end if
        szBuff[msg_len] = '\0'; // newly added at Feb 14, 2025
        printf("Bytes Received: %d, message: %s from %s\n", msg_len, szBuff, inet_ntoa(client_addr.sin_addr));

        msg_len = send(msg_sock, szBuff, msg_len, 0); // newly modified at Feb 14, 2025
        if (msg_len == 0) {
            printf("Client closed connection\n");
            closesocket(msg_sock);
            return -1;
        } // end if

    } // end while loop

    closesocket(msg_sock);
    WSACleanup();
    return 0;
}
