#pragma once
#include <mutex>
#include <iostream>
#include "../Common/CommonFuction.h"

#define TEXT_GREEN 32
#define TEXT_YELLOW 33

class Console
{
private:
	inline static std::mutex m_lock;
	inline static std::mutex m_wlock;

public:
	inline static std::ostream& Begin(unsigned long timeout = 1000)
	{
		if (WaitToLock(m_lock, 16, timeout))
		{
			return std::cout;
		}
	}

	inline static std::wostream& BeginW(unsigned long timeout = 1000)
	{
		if (WaitToLock(m_wlock, 16, timeout))
		{
			return std::wcout;
		}
	}

	inline static void End()
	{
		Release(m_lock);
	}

	inline static void EndW()
	{
		Release(m_wlock);
	}

	inline static void Set(int opt)
	{
		std::cout << "\033[1;" << std::to_string(opt).c_str() << "m";
	}

	inline static void SetYellowText()
	{
		std::cout << "\033[1;33m";
	}

	inline static void SetWhiteText()
	{
		std::cout << "\033[0m";
	}
};

#define ConsoleLog(msg) Console::Begin() << msg; Console::End()
