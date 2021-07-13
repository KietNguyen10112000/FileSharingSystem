#pragma once
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>

inline void LogSSL()
{
	int err;
	while (err = ERR_get_error())
	{
		char* str = ERR_error_string(err, 0);
		if (!str)
			return;
		std::cout << str << "\n";
		std::cout << fflush;
	}
}