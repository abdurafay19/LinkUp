// client2_windows.cpp
#include <iostream> // For input/output
#include <thread> // For multi-threading
#include <string> // For strings
#include <winsock2.h> // Windows Sockets API
#include <ws2tcpip.h> // For InetPtonA and other extended IP functions

#pragma comment(lib, "ws2_32.lib") // Link with Winsock library

#define PEER_IP "127.0.0.1"  // IP of client1
#define PEER_PORT 5000 // PORT of client1

// Runs in its own thread to continuously receive messages from the peer
void receiveMessages(SOCKET sock)
{
    char buffer[1024];
    int bytesReceived;
    while (true)
    {
        bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0); // reads data into buffer
        if (bytesReceived <= 0) break;
        buffer[bytesReceived] = '\0';
        std::cout << "\nPeer: " << buffer << std::endl;
    }
}

// Runs in its own thread and reads user input from the console and sends it to the peer
void sendMessages(SOCKET sock)
{
    std::string msg;
    while (true)
    {
        std::getline(std::cin, msg);
        send(sock, msg.c_str(), msg.length(), 0); // sends the message as a C-string over the socket
    }
}

int main()
{
    WSADATA wsaData; // holds information about the Winsock implementation
    SOCKET sock; // the socket used to communicate
    sockaddr_in serverAddr; // the address of the peer

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    // Creates a TCP socket, AF_INET: IPv4
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PEER_PORT);

    int result = InetPtonA(AF_INET, PEER_IP, &serverAddr.sin_addr);
    if (result != 1) {
        std::cerr << "Invalid IP address format" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Connect to server
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to peer!" << std::endl;

    std::thread recvThread(receiveMessages, sock);
    std::thread sendThread(sendMessages, sock);

    recvThread.join();
    sendThread.join();

    closesocket(sock);
    WSACleanup();

    return 0;
}
