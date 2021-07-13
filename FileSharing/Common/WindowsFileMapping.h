#include <windows.h>

#pragma comment(lib, "user32.lib")

#include <iostream>
#include <string>

#define BUF_SIZE 1024

//listener will listen request and display to console, 
//console of listener will display only and user must use console of responder to make a response
//responder will receive info from listener

struct __LocalConnection
{
    std::wstring sendName = L"Local\\ListenerSend";
    std::wstring recvName = L"Local\\ListenerRecv";

    HANDLE hSendMapFile = nullptr;
    HANDLE hRecvMapFile = nullptr;
    char* sendBuf = nullptr;
    char* recvBuf = nullptr;

};


inline int InitSend(__LocalConnection& conn)
{
    if (conn.hRecvMapFile) return -1;

    //=================================================================================================================
    //init send buffer
    conn.hSendMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security
        PAGE_READWRITE,          // read/write access
        0,                       // maximum object size (high-order DWORD)
        BUF_SIZE + 2,                // maximum object size (low-order DWORD)
        conn.sendName.c_str());               // name of mapping object

    if (conn.hSendMapFile == NULL)
    {
        return GetLastError();
    }

    conn.sendBuf = (char*)MapViewOfFile(conn.hSendMapFile,   // handle to map object
        FILE_MAP_ALL_ACCESS, // read/write permission
        0,
        0,
        BUF_SIZE);

    if (conn.sendBuf == NULL)
    {
        CloseHandle(conn.hSendMapFile);
        return GetLastError();
    }

    memset(conn.sendBuf, 0, BUF_SIZE + 2);

    return 0;
}

inline int InitRecv(__LocalConnection& conn)
{
    if (conn.hRecvMapFile) return -1;

    //=================================================================================================================
    //init recv buffer
    conn.hRecvMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,   // read/write access
        FALSE,                 // do not inherit the name
        conn.recvName.c_str());               // name of mapping object

    if (conn.hRecvMapFile == NULL)
    {
        return GetLastError();
    }

    conn.recvBuf = (char*)MapViewOfFile(conn.hRecvMapFile, // handle to map object
        FILE_MAP_ALL_ACCESS,  // read/write permission
        0,
        0,
        BUF_SIZE + 2);

    if (conn.recvBuf == NULL)
    {
        CloseHandle(conn.hRecvMapFile);
        return GetLastError();
    }

    return 0;
}

inline void FreeConnection(__LocalConnection& conn)
{
    if (conn.sendBuf) UnmapViewOfFile(conn.sendBuf);
    if (conn.hSendMapFile) CloseHandle(conn.hSendMapFile);
    if (conn.recvBuf) UnmapViewOfFile(conn.recvBuf);
    if (conn.hRecvMapFile) CloseHandle(conn.hRecvMapFile);
}

#define _BUFFER_EMPTY -3
#define _BUFFER_OVERFLOW -2
#define _BUFFER_NOT_EMPTY -1
#define _SUCCESS 0

//size in byte, size of buffer
//send buffer to local file mapping
inline int LocalSend(__LocalConnection& conn, const char* buffer, size_t size)
{
    if (conn.sendBuf[0] != 0) return _BUFFER_NOT_EMPTY;
    if (size > BUF_SIZE) return _BUFFER_OVERFLOW;
    memcpy(conn.sendBuf, buffer, size);
    return _SUCCESS;
}

//size in byte, size of buffer
//copy recv buffer to buffer
inline int LocalRecv(__LocalConnection& conn, char* buffer, size_t size)
{
    if (size < BUF_SIZE) return _BUFFER_OVERFLOW;
    //if (conn.recvBuf[0] == 0) return _BUFFER_EMPTY;
    memcpy(buffer, conn.recvBuf, BUF_SIZE);
    conn.recvBuf[0] = 0;
    conn.recvBuf[1] = 0;
    return _SUCCESS;
}

inline bool ReadyToSend(__LocalConnection& conn)
{
    return conn.sendBuf[0] == 0 && conn.sendBuf[1] == 0;
}

inline bool ReadyToRecv(__LocalConnection& conn)
{
    return conn.recvBuf[0] != 0 || conn.recvBuf[1] != 0;
}

inline bool IsEndRecv(__LocalConnection& conn)
{
    return conn.recvBuf[BUF_SIZE] == '\0' && conn.recvBuf[BUF_SIZE + 1] == '\0';
}

inline void StartSend(__LocalConnection& conn)
{
    conn.sendBuf[BUF_SIZE] = 's';
    conn.sendBuf[BUF_SIZE + 1] = 's';
}

inline void EndSend(__LocalConnection& conn)
{
    while (conn.sendBuf[0] != 0)
    {
        Sleep(16);
    }

    conn.sendBuf[BUF_SIZE] = '\0';
    conn.sendBuf[BUF_SIZE + 1] = '\0';
}


