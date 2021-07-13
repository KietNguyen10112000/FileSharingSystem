#pragma once
#include <string>
#include <vector>

#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

struct AdapterInfor
{
    std::wstring desc;
    std::wstring name;
    std::wstring ipv4;
    std::wstring ipv6;

    friend void operator<<(std::wostream& out, AdapterInfor& adapter)
    {
        out << "[ADAPTER]\t" << adapter.desc << "\n[NAME]\t\t"
            << adapter.name << "\n[IPv4]\t\t" << adapter.ipv4 << "\n[IPv6]\t\t" << adapter.ipv6 << "\n";
    };

};

std::vector<AdapterInfor> GetAdaptersInformation()
{
    std::vector<AdapterInfor> result;
    IP_ADAPTER_ADDRESSES* adapter_addresses(NULL);
    IP_ADAPTER_ADDRESSES* adapter(NULL);

    DWORD adapter_addresses_buffer_size = 16 * 1024;

    // Get adapter addresses
    for (int attempts = 0; attempts != 3; ++attempts)
    {
        adapter_addresses = (IP_ADAPTER_ADDRESSES*)malloc(adapter_addresses_buffer_size);

        DWORD error = ::GetAdaptersAddresses(AF_UNSPEC,
            GAA_FLAG_SKIP_ANYCAST |
            GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER |
            GAA_FLAG_SKIP_FRIENDLY_NAME,
            NULL,
            adapter_addresses,
            &adapter_addresses_buffer_size);

        if (ERROR_SUCCESS == error)
        {
            break;
        }
        else if (ERROR_BUFFER_OVERFLOW == error)
        {
            // Try again with the new size
            free(adapter_addresses);
            adapter_addresses = NULL;
            continue;
        }
        else
        {
            // Unexpected error code - log and throw
            free(adapter_addresses);
            adapter_addresses = NULL;
            throw "GET ADAPTERS INFORMATION ERROR!!!";
        }
    }

    // Iterate through all of the adapters
    for (adapter = adapter_addresses; NULL != adapter; adapter = adapter->Next)
    {
        // Skip loopback adapters
        if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType) continue;

        AdapterInfor infor;

        infor.name = adapter->FriendlyName;
        infor.desc = adapter->Description;

        // Parse all IPv4 addresses
        for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress; NULL != address; address = address->Next)
        {
            auto family = address->Address.lpSockaddr->sa_family;
            if (AF_INET == family)
            {
                SOCKADDR_IN* ipv4 = reinterpret_cast<SOCKADDR_IN*>(address->Address.lpSockaddr);
                wchar_t str_buffer[16] = { 0 };
                InetNtop(AF_INET, &(ipv4->sin_addr), str_buffer, 16);

                //string ip = str_buffer;
                infor.ipv4 = str_buffer;
            }

            if (AF_INET6 == family)
            {
                SOCKADDR_IN6* ipv6 = reinterpret_cast<SOCKADDR_IN6*>(address->Address.lpSockaddr);
                wchar_t str_buffer[128] = { 0 };
                InetNtop(AF_INET6, &(ipv6->sin6_addr), str_buffer, 128);
                infor.ipv6 = str_buffer;
            }
        }
        result.push_back(infor);
    }

    free(adapter_addresses);
    adapter_addresses = NULL;

    return result;
}