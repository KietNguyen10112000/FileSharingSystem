#include <iostream>
#include <string>

#include <conio.h>

#include "Connection.h"
#include <openssl\ssl.h>

#include "../Common/WindowsFileMapping.h"
#include "../Common/CommonFuction.h"
#include "Server.h"
#include "CommonSSL.h"
#include "UDPConnection.h"
#include "FileSharingHost.h"

//__LocalConnection _localConnection;
//
//__LocalConnection InitConnection()
//{
//    __LocalConnection conn;
//
//    conn.sendName = L"Local\\FileSharingBuffer1";
//    conn.recvName = L"Local\\FileSharingBuffer2";
//
//    auto re = InitSend(conn);
//    if (re != 0)
//    {
//        FreeConnection(conn);
//        Throw(L"InitSend() error.");
//    }
//
//    std::string msg = "Hello world from host.";
//    LocalSend(conn, msg.c_str(), msg.size());
//
//    std::wcout << L"Wait for local connection ...\n";
//    while (conn.sendBuf[0] != 0)
//    {
//        Sleep(32);
//    }
//
//    re = InitRecv(conn);
//    if (re != 0)
//    {
//        FreeConnection(conn);
//        Throw(L"InitRecv() error.\n");
//    }
//
//    char* buffer = new char[BUF_SIZE]();
//    LocalRecv(conn, buffer, BUF_SIZE);
//    std::cout << buffer << "\n";
//    delete[] buffer;
//
//    return conn;
//}

void SSLClientDemo()
{
    Connection* conn = nullptr;
    try
    {
        std::string host;
        std::getline(std::cin, host);

        std::string strport = host.substr(host.find(':') + 1);
        u_short port = 0;
        if (!strport.empty()) port = std::stoi(strport);

        conn = new Connection(host, port, CONNECTION_OPT::IPv4);

        std::string msg = "GET https://www.google.com.vn HTTP/1.1\r\n\r\n";
        std::getline(std::cin, msg);

        std::string buffer;
        buffer.resize(2048);
        //142.250.204.67

        if (conn->Send(msg.c_str(), msg.size()) != 0)
        {
            Throw(L"Send() error.");
        }

        conn->Recv(&buffer[0], buffer.size());

        std::cout << buffer << "\n";

    }
    catch (const wchar_t* e)
    {
        std::wcout << e << L"\n";
    }

    system("pause");

    SafeDelete(conn);
}

//void SSLServerDemo()
//{
//    Server* server = nullptr;
//    try
//    {
//        /*std::string host;
//        std::getline(std::cin, host);
//
//        std::string strport = host.substr(host.find(':') + 1);
//        u_short port = 0;
//        if (!strport.empty()) port = std::stoi(strport);*/
//
//        server = new Server("", 443, SERVER_OPT::SERVER_IPv4);
//
//        //142.250.204.67
//        std::string msg = "<!doctype html><html><body><div><h1>Example Domain</h1></div></body></html>";
//
//        server->Listen([&](Server* server, ServerThread* servThread, ClientConnection** pConn)
//            {
//                auto conn = *pConn;
//                if (IsReadyRead(conn->sock, 0, 16))
//                {
//                    auto re = conn->Recv(&conn->buffer[0], conn->bufferSize);
//
//                    if (re == -1)
//                    {
//                        //LogSSL();
//                        server->Close(conn);
//                        return;
//                    }
//                    if (re > 0)
//                    {
//                        std::cout << conn->buffer << "\n";
//                    }
//
//                }
//
//                if (IsReadyWrite(conn->sock, 0, 16))
//                {
//                    if (conn->buffer[0] != 0)
//                    {
//                        conn->buffer[0] = 0;
//                        auto re = conn->Send(msg.c_str(), msg.size());
//                        if (re == -1)
//                        {
//                            server->Close(conn);
//                            return;
//                        }
//                        if (re == msg.size())
//                        {
//                            Sleep(300);
//                            server->Close(conn);
//                            std::cout << "Send().\n";
//                        }
//                    }
//
//                }
//                /*else
//                {
//                    server->Close(conn);
//                }*/
//
//            });
//
//    }
//    catch (const wchar_t* e)
//    {
//        std::wcout << e << L"\n";
//    }
//    catch (const std::wstring e1)
//    {
//        std::wcout << e1 << L"\n";
//    }
//
//    system("pause");
//
//    SafeDelete(server);
//}

void BroadCastDemo()
{
    u_short sport = 0;

    std::cout << "Config \nBroadcast port: ";
    std::cin >> sport;

    std::cout << "Destination: ";

    std::string host;
    std::getline(std::cin, host);
    std::getline(std::cin, host);

    std::string strport = host.substr(host.find(':') + 1);
    u_short port = 0;
    if (!strport.empty()) port = std::stoi(strport);

    UDPConnection* broadcast = new UDPConnection(sport, "192.168.1.255", port);

    std::string msg = "Hello world from " + std::to_string(sport);
    std::string buffer;
    buffer.resize(1024);

    broadcast->SetMsg(&msg[0], msg.size() + 1);
    broadcast->SetRecvBuffer(&buffer[0], buffer.size());

    std::string srcDesc;
    srcDesc.resize(256);

    broadcast->Loop(
        [&]()
        {
            std::cout << "send().\n";
            Sleep(3000);
        },
        [&](sockaddr_in6* src, int numbytes)
        {
            InetNtopA(AF_INET, &((u_short*)src)[2], &srcDesc[0], srcDesc.size());
            std::cout << "\nReceive from: " << (char*)&srcDesc[0] << ":" << ntohs(src->sin6_port) << "\nMsg: " << (char*)&buffer[0] << "\n\n";

        });

    SafeDelete(broadcast);
}

void HttpsRequestDemo()
{
    std::cout << "Host: ";
    std::string host;
    std::getline(std::cin, host);

    //std::string strport = host.substr(host.find(':') + 1);
    //u_short port = 0;
    //if (!strport.empty()) port = std::stoi(strport);

    std::string msg = "GET / HTTP/1.1\n\
Host: www.google.com\n\
User-Agent: curl/7.55.1\n\
Accept: */*\n\n\n\n";

    //std::getline(std::cin, msg);
    try
    {
        std::cout << Request(host, 443, IPv6, msg) << "\n";
    }
    catch (const wchar_t* e)
    {
        std::wcout << e << L"\n";
    }
    catch (const std::wstring e1)
    {
        std::wcout << e1 << L"\n";
    }

}

int main()
{
    InitConsole();
    /*InitConsole();
    try
    {
        _localConnection = InitConnection();

        char* buffer = new char[BUF_SIZE]();

        while (true)
        {
            if (!ReadyToRecv(_localConnection)) Sleep(32);
            else
            {
                LocalRecv(_localConnection, buffer, BUF_SIZE);
                std::wstring msg = (wchar_t*)buffer;
                std::wcout << msg << L"\n";
            }
        }

        delete[] buffer;
    }
    catch (const wchar_t* e)
    {
        std::wcout << e << L"\n";
    }

    FreeConnection(_localConnection);*/



    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    SSL_load_error_strings();
    //SSLeay_add_ssl_algorithms();

    //SSLServerDemo();

    //SSLClientDemo();

    //BroadCastDemo();

    //auto lo = std::cout.imbue(std::locale("en_US.utf8"));

    //HttpsRequestDemo();

    FileSharingHost host;

    host.Launch();

    WSACleanup();

    return 0;
}