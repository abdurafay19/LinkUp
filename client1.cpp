// client1_windows.cpp
#include <iostream> // For input/output
#include <thread> // For multi-threading
#include <string> // For strings
#include <winsock2.h> // Windows Sockets API
#include <ws2tcpip.h> // For InetPtonA and other extended IP functions

#pragma comment(lib, "ws2_32.lib") // Link with Winsock library

#define PORT 5000

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
    WSADATA wsaData;
    SOCKET serverSocket, newSocket;
    sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    // Set up the server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind socket
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 1) == SOCKET_ERROR)
    {
        std::cerr << "Listen failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Waiting for connection on port " << PORT << "...\n";

    newSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
    if (newSocket == INVALID_SOCKET)
    {
        std::cerr << "Accept failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to peer!" << std::endl;

    std::thread recvThread(receiveMessages, newSocket);
    std::thread sendThread(sendMessages, newSocket);

    recvThread.join();
    sendThread.join();

    closesocket(newSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
