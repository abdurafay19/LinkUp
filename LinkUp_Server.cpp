#include <winsock2.h>    // Core Windows Sockets APIs
#include <ws2tcpip.h>    // getaddrinfo, IPv6, and TCP/IP definitions
#include <iostream>      // std::cout, std::endl
#include <string>        // std::string
#include <thread>        // std::thread
#include <mutex>         // std::mutex, std::lock_guard
#include <unordered_map> // std::unordered_map for key-value storage
#include <vector>        // std::vector for dynamic arrays
#include <sstream>       // std::istringstream for parsing strings
#include <fstream>      // For file I/O
#include <exception>    // For std::exception
#include <chrono>

#pragma comment(lib, "ws2_32.lib")  // Link against Winsock2 library

using namespace std;

//----------------------------------------------------------------------
// Data structure to hold each client's credentials and connection info
//---------------------------------------------------------------------- 
struct ClientInfo
{
    string username;   // Unique user identifier
    string password;   // Password (in production, should be hashed!)
    string ip_port;    // Stored "IP:Port" so peers can connect P2P
    chrono::steady_clock::time_point lastHeartbeat;
};

//----------------------------------------------------------------------
// Global in-memory database of registered clients
// Protected by a mutex to allow safe concurrent access
//---------------------------------------------------------------------- 
unordered_map<string, ClientInfo> clientDB;
mutex dbMutex;

//----------------------------------------------------------------------
// A simple application-level code to prevent unauthorized API calls.
// In a real system this would be replaced by TLS, API tokens, etc.
//---------------------------------------------------------------------- 
const string APP_CODE = "SECRET_APP_123";

//----------------------------------------------------------------------
// File path for persistence
//---------------------------------------------------------------------- 
const string DB_FILE = "client_db.txt";

//----------------------------------------------------------------------
// Utility: Split a string by delimiter, returns vector of tokens.
// Used to parse our simple 'CODE|ACTION|USERNAME|PASSWORD|IP_PORT' protocol.
//---------------------------------------------------------------------- 
vector<string> splitString(const string& input, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(input);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

//----------------------------------------------------------------------
// Load clientDB from file at startup
//---------------------------------------------------------------------- 
void loadClientDB()
{
    // First check if file exists; if not, create it
    std::ifstream testFile(DB_FILE);
    if (!testFile) {
        std::cerr << "[Warning] " << DB_FILE << " not found. Creating a new one...\n";
        std::ofstream createFile(DB_FILE);
        if (!createFile) {
            std::cerr << "[Error] Could not create " << DB_FILE << std::endl;
            return;
        }
        createFile.close();
    }
    else {
        testFile.close();
    }

    std::ifstream dbFile;
    try
    {
        // Enable exceptions only after we know the file exists
        dbFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        dbFile.open(DB_FILE);
        std::string line;
        while (std::getline(dbFile, line))
        {
            auto parts = splitString(line, '|');
            if (parts.size() == 3) {
                clientDB[parts[0]] = { parts[0], parts[1], parts[2] };
            }
        }
        dbFile.close();
    }
    catch (const std::ios_base::failure& e)
    {
        std::cerr << "[Warning] Could not open or read " << DB_FILE
            << ": " << e.what() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Error] Exception loading DB: " << e.what() << std::endl;
    }
}


//----------------------------------------------------------------------
// Save entire clientDB to file
//---------------------------------------------------------------------- 
void saveClientDB()
{
    ofstream dbFile;
    try
    {
        dbFile.exceptions(ofstream::failbit | ofstream::badbit);
        dbFile.open(DB_FILE, ios::trunc);
        for (const auto& entry : clientDB)
        {
            const auto& info = entry.second;
            dbFile << info.username << '|'
                << info.password << '|'
                << info.ip_port << '\n';
        }
        dbFile.close();
    }
    catch (const ios_base::failure& e)
    {
        cerr << "[Error] Could not write to " << DB_FILE
            << ": " << e.what() << endl;
    }
    catch (const exception& e)
    {
        cerr << "[Error] Exception saving DB: " << e.what() << endl;
    }
}

//----------------------------------------------------------------------
// Function: handleClient
// Purpose:  Runs in its own thread to process a single client's request.
//---------------------------------------------------------------------- 
void handleClient(SOCKET clientSocket)
{
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0)
    {
        closesocket(clientSocket);
        return;
    }

    // Read request and split by '|'
    string request(buffer, bytesReceived);
    vector<string> parts = splitString(request, '|');
    if (parts.size() < 2)
    {
        send(clientSocket, "ERROR|Invalid format", 20, 0);
        closesocket(clientSocket);
        return;
    }

    string code = parts[0];  // APP_CODE
    string action = parts[1];  // REG, LOG, PULS, SRCH

    // Verify application code
    if (code != APP_CODE)
    {
        send(clientSocket, "ERROR|Invalid application code", 29, 0);
        closesocket(clientSocket);
        return;
    }

    // Determine origin username (always parts[2] for all actions)
    string originUser = (parts.size() >= 3 ? parts[2] : "");

    // For PULS and SRCH, ensure originUser is already online
    if (action == "PULS" || action == "SRCH")
    {
        auto itO = clientDB.find(originUser);
        if (itO == clientDB.end() || itO->second.ip_port == "null")
        {
            send(clientSocket, "ERROR|User not online", 21, 0);
            closesocket(clientSocket);
            return;
        }
    }

    lock_guard<mutex> lock(dbMutex);
    string response;

    if (action == "REG")
    {
        // Expect: CODE|REG|username|password|ip:port
        if (parts.size() < 5)
        {
            response = "ERROR|Invalid format";
        }
        else {
            string username = parts[2];
            string password = parts[3];
            string ip_port = parts[4];
            if (clientDB.count(username))
            {
                response = "ERROR|Username exists";
            }
            else
            {
                clientDB[username] =
                {
                    username, password, ip_port,
                    chrono::steady_clock::now()
                };
                saveClientDB();
                response = "SUCCESS|Registration successful";
            }
        }
    }
    else if (action == "LOG")
    {
        // Expect: CODE|LOG|username|password|ip:port
        if (parts.size() < 5)
        {
            response = "ERROR|Invalid format";
        }
        else {
            string username = parts[2];
            string password = parts[3];
            string ip_port = parts[4];
            auto it = clientDB.find(username);
            if (it != clientDB.end() && it->second.password == password)
            {
                it->second.ip_port = ip_port;
                it->second.lastHeartbeat = chrono::steady_clock::now();
                saveClientDB();
                response = "SUCCESS|Login successful";
            }
            else
            {
                response = "ERROR|Invalid credentials";
            }
        }
    }
    else if (action == "PULS")
    {
        // Expect: CODE|PULS|username
        // originUser already validated as online
        auto it = clientDB.find(originUser);
        it->second.lastHeartbeat = chrono::steady_clock::now();
        response = "SUCCESS|Pulse received";
    }
    else if (action == "SRCH")
    {
        // Expect: CODE|SRCH|originUsername|targetUsername
        if (parts.size() < 4) {
            response = "ERROR|Invalid format";
        }
        else {
            string target = parts[3];
            auto itT = clientDB.find(target);
            if (itT != clientDB.end())
            {
                if (itT->second.ip_port != "null")
                {
                    response = "SUCCESS|" + itT->second.ip_port;
                }
                else
                {
                    response = "ERROR|User offline";
                }
            }
            else
            {
                response = "ERROR|User not found";
            }
        }
    }
    else if (action == "DISC")
    {
        // Expect: CODE|DISC|username
        auto it = clientDB.find(originUser);
        if (it != clientDB.end())
        {
            it->second.ip_port = "null";
            response = "SUCCESS|Disconnected successfully";
        }
        else
        {
            response = "ERROR|User not found";
        }
    }
    else
    {
        response = "ERROR|Invalid action";
    }

    send(clientSocket, response.c_str(), (int)response.size(), 0);
    closesocket(clientSocket);
}


//----------------------------------------------------------------------
// Entry point: sets up Winsock, listens for incoming connections,
// and spawns threads to handle each client.
//---------------------------------------------------------------------- 
int main()
{
    // Initialize Winsock version 2.2
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    loadClientDB();

    // Start heartbeat monitor
    const auto HEARTBEAT_TIMEOUT = chrono::seconds(30);
    thread([&]()
        {
        while (true)
        {
            this_thread::sleep_for(chrono::seconds(5));
            lock_guard<mutex> lock(dbMutex);
            auto now = chrono::steady_clock::now();
            for (auto& p : clientDB)
            {
                auto& info = p.second;
                if (info.ip_port != "null" &&
                    now - info.lastHeartbeat > HEARTBEAT_TIMEOUT)
                {
                    info.ip_port = "null";
                }
            }
        }
        }).detach();

    // Create a TCP socket (IPv4)
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Configure the server address structure
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;             // IPv4
    serverAddr.sin_port = htons(8080);         // Port 8080, network byte order
    serverAddr.sin_addr.s_addr = INADDR_ANY;          // Bind to all local interfaces

    // Bind socket to address and port
    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    // Start listening, backlog = max connections
    listen(serverSocket, SOMAXCONN);

    cout << "Server running on port 8080..." << endl;

    // Main accept loop: runs indefinitely
    while (true) {
        // Accept blocks until a new connection comes in
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket != INVALID_SOCKET)
        {
            // Spawn detached thread to handle this client
            thread(handleClient, clientSocket).detach();
        }
    }

    // Cleanup (unreachable in current design)
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
