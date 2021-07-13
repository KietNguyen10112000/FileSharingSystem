#include <iostream>
#include <string>

#include "../Common/WindowsFileMapping.h"
#include "../Common/CommonFuction.h"
#include "../Common/File.h"

#include <filesystem>

namespace fs = std::filesystem;

__LocalConnection _localConnection;

std::wstring currentDir;

__LocalConnection InitConnection()
{
    __LocalConnection conn;

    conn.sendName = L"Local\\FileSharingBuffer2";
    conn.recvName = L"Local\\FileSharingBuffer1";

    auto re = InitSend(conn);
    if (re != 0)
    {
        FreeConnection(conn);
        Throw(L"InitSend() error.");
    }

    re = InitRecv(conn);
    if (re != 0)
    {
        FreeConnection(conn);
        Throw(L"InitRecv() error.");
    }

    char* buffer = new char[BUF_SIZE]();
    LocalRecv(conn, buffer, BUF_SIZE);
    std::cout << buffer << "\n";
    delete[] buffer;

    std::string msg = "Hello world from client.";
    LocalSend(conn, msg.c_str(), msg.size());

    return conn;
}

void ShowConsoleCursor(bool showFlag)
{
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_CURSOR_INFO     cursorInfo;

    GetConsoleCursorInfo(out, &cursorInfo);
    cursorInfo.bVisible = showFlag; // set the cursor visibility
    SetConsoleCursorInfo(out, &cursorInfo);
#endif // _WINDOW_
}

#define MAX_INPUT_LENGTH 1024

void ChangeDirectory(std::wstring param)
{
    StandardPath(param);

    if (fs::is_directory(param))
    {
        currentDir = CombinePath(currentDir, param);

        if (param.back() == '\\')
        {
            currentDir.pop_back();
        }
        SetCurrentDir(currentDir);
    }

}

int main()
{
    InitConsole();
    std::wcout << std::fixed << std::setprecision(2) << std::setfill(L' ');
    SetConsoleTitle(L"File Sharing System");

    wchar_t* recvBuff = new wchar_t[BUF_SIZE + 1]();

    std::string password = "";
    std::wstring cmd;

    wchar_t buffer[_MAX_DIR] = {};
    GetCurrentDirectoryW(_MAX_DIR, buffer);
    currentDir = buffer;

    std::wstring prefix;
    size_t* pCurrent = 0;
    size_t* pTotal = 0;
    std::wstring unit;

#ifdef _WIN32
    void* con = GetStdHandle(STD_INPUT_HANDLE);

    unsigned long read;
    wchar_t* wstr = new wchar_t[MAX_INPUT_LENGTH]();
#endif // _WIN32

    try
    {
        _localConnection = InitConnection();

        std::wstring msg = L"cd " + currentDir;

        while (!ReadyToSend(_localConnection))
        {
            Sleep(16);
        }

        LocalSend(_localConnection, (char*)msg.c_str(), (msg.size() + 1) * sizeof(wchar_t));

        while (!ReadyToRecv(_localConnection))
        {
            Sleep(16);
        }

        _localConnection.recvBuf[0] = 0;

        while (true)
        {
            std::wcout << currentDir << L">";
#ifdef _WIN32
            ReadConsole(con, wstr, MAX_INPUT_LENGTH, &read, NULL);
            wstr[read - 1] = L'\0';
            wstr[read - 2] = L'\0';
            msg = wstr;
#endif
            while (!ReadyToSend(_localConnection))
            {
                Sleep(16);
            }
            std::wcout << L'\n';
            cmd = msg.substr(0, msg.find(L' '));
            if (msg.empty()) continue;
            ToLower(cmd);
            if (cmd == L"cd")
            {
                if (cmd.size() + 2 > msg.size())
                {
                    std::cout << "Invalid.\n";
                    continue;
                }
                ChangeDirectory(&msg[cmd.size() + 1]);
            }

            if (cmd == L"quit")
            {
                LocalSend(_localConnection, (char*)msg.c_str(), (msg.size() + 1) * sizeof(wchar_t));
                break;
            }

            if (cmd == L"system")
            {
                _wsystem(msg.substr(msg.find_first_not_of(L' ', 7)).c_str());
                continue;
            }

            if (cmd == L"file-system-protection")
            {
                //std::wcout << "open-file-protection\n";
                SetConsoleTitle(L"File System Protection");
                _wsystem((std::wstring(buffer) + L"\\FileSystemProtection.exe").c_str());
                SetConsoleTitle(L"File Sharing System");
            }
            else
            {
                LocalSend(_localConnection, (char*)msg.c_str(), (msg.size() + 1) * sizeof(wchar_t));

                while (!ReadyToRecv(_localConnection))
                {
                    Sleep(16);
                }

                while (!IsEndRecv(_localConnection))
                {
                    if (ReadyToRecv(_localConnection))
                    {
                        if (LocalRecv(_localConnection, (char*)recvBuff, BUF_SIZE) == _SUCCESS)
                        {
                            if (recvBuff[0] == WCHAR_MAX)
                            {
                                if (recvBuff[1] == WCHAR_MAX - 1)
                                {
                                    //input password
                                    password = InputPassword();
                                    LocalSend(_localConnection, &password[0], (password.size() + 1));
                                    std::string em = "";
                                    password.swap(em);
                                }
                                else if (recvBuff[1] == WCHAR_MAX - 2)
                                {
                                    currentDir = &recvBuff[2];
                                    LocalSend(_localConnection, (char*)msg.c_str(), (msg.size() + 1) * sizeof(wchar_t));
                                }
                                else if (recvBuff[1] == WCHAR_MAX - 3)
                                {
                                    std::wcout << &recvBuff[2];
                                    ConsoleGetLine(wstr, MAX_INPUT_LENGTH);
                                    msg = wstr;
                                    LocalSend(_localConnection, (char*)msg.c_str(), (msg.size() + 1) * sizeof(wchar_t));
                                }
                                else if (recvBuff[1] == WCHAR_MAX - 4)
                                {
                                    wchar_t* buff = (wchar_t*)_localConnection.recvBuf;
                                    prefix = &recvBuff[2];
                                    unit = &recvBuff[3 + prefix.size()];
                                    pCurrent = (size_t*)&buff[6];
                                    pTotal = (size_t*)&buff[2];
                                }
                                else if (recvBuff[1] == WCHAR_MAX - 5)
                                {
                                    ShowConsoleCursor(false);

                                    std::cout << "\033[1;32m";

                                    bool isFirst = true;
     
                                    wchar_t* buff = (wchar_t*)_localConnection.recvBuf;

                                    while (buff[1] == WCHAR_MAX - 5)
                                    {
                                        if (isFirst)
                                        {
                                            isFirst = false;
                                            ((wchar_t*)_localConnection.sendBuf)[0] = WCHAR_MAX;
                                        }

                                        std::wcout << L"\r" << prefix << *pCurrent << L"/" << *pTotal
                                            << L' ' << unit << L' ' << (*pCurrent / (float)*pTotal) * 100 << L" %                        ";
                                        Sleep(16);
                                    }
                                    std::wcout << L"\r" << prefix << *pCurrent << L"/" << *pTotal
                                        << L' ' << unit << L' ' << (*pCurrent / (float)*pTotal) * 100 << L" %                    \n";

                                    ((wchar_t*)_localConnection.sendBuf)[0] = 1;

                                    std::cout << "\033[0m";

                                    ShowConsoleCursor(true);

                                }
                                else if (recvBuff[1] == WCHAR_MAX - 6)
                                {
                                    wchar_t* buff = (wchar_t*)_localConnection.recvBuf;

                                    while (buff[1] != WCHAR_MAX - 6)
                                    {
                                        Sleep(16);
                                    }

                                    while (buff[1] == WCHAR_MAX - 6)
                                    {
                                        if (buff[2] != 0)
                                        {
                                            std::wcout << &buff[2];
                                            buff[2] = 0;
                                        }
                                        else Sleep(16);
                                    }
                                    std::wcout << L"\n";
                                }
                            }
                            else
                            {
                                std::wcout << recvBuff;
                            }
                        }
                    }
                    else
                    {
                        Sleep(16);
                    }
                }
                std::wcout << L'\n';
            }
            
            std::wcout << L'\n';
            
        }

    }
    catch (const wchar_t* e)
    {
        std::wcout << e << L"\n";
    }

#ifdef _WIN32
    delete[] wstr;
#endif

    FreeConnection(_localConnection);

    system("pause");

    return 0;
}