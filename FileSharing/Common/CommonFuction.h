#pragma once
#include <codecvt>
#include <string>
#include <algorithm>
#include <vector>
#include <chrono>
#include <iostream>
#include <mutex>

#include <Windows.h>

#ifdef _DEBUG
#ifndef Throw
#define Throw(msg) throw msg L"\nThrow from File \"" __FILEW__ L"\", Line " _STRINGIZE(__LINE__) L"."
#endif // !Throw
#include <cassert>
#else
#ifndef Throw
#define Throw(msg) throw msg
#endif // !Throw
#include <cassert>
#endif // DEBUG
#include <conio.h>

#define SafeDelete(p) if(p) { delete p; p = nullptr; }

template<class Base, class T>
inline bool instanceof(T* ptr) {
	try
	{
		return dynamic_cast<Base*>(ptr) != nullptr;
	}
	catch (const std::exception& e)
	{
		return false;
	}
}

//#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
//#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
//#define _CRT_SECURE_NO_WARNINGS
//#pragma error(suppress : 4996)
inline std::string WStringToString(const std::wstring& wstr)
{
	std::string convertedStr('c', wstr.length());
	
	int convertResult = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr.length(), 
		convertedStr.data(), convertedStr.length(), NULL, NULL);
	if (convertResult >= 0)
	{
		std::string result(&convertedStr[0], &convertedStr[convertResult]);
		//convertedStr[convertResult] = '\0';

		if (result.empty()) return convertedStr;
		
		return result;
	}
	else
	{
		return "";
	}
	
}

inline std::wstring StringToWString(const std::string& str)
{
	std::wstring re;
	re.resize(str.size());
	for (size_t i = 0; i < str.length(); i++)
	{
		re[i] = str[i];
	}

	return re;
}

inline void ToLower(std::string& strToConvert)
{
	std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(),
		[](unsigned char c) { return std::tolower(c); });
}

inline void ToUpper(std::string& strToConvert)
{
	for (unsigned int i = 0; i < strToConvert.length(); i++)
	{
		strToConvert[i] = toupper(strToConvert[i]);
	}
}

inline void ToLower(std::wstring& strToConvert)
{
	std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(),
		[](wchar_t c) { return std::tolower(c); });
}

inline void ToUpper(std::wstring& strToConvert)
{
	for (unsigned int i = 0; i < strToConvert.length(); i++)
	{
		strToConvert[i] = toupper(strToConvert[i]);
	}
}

inline void StringSplit(std::string s, std::string delimiter, std::vector<std::string>& out)
{
	out.clear();
	size_t pos = 0;
	std::string token;
	while ((pos = s.find(delimiter)) != std::string::npos) {
		token = s.substr(0, pos);
		if (!token.empty())
			out.push_back(token);
		s.erase(0, pos + delimiter.length());
	}
	if (!s.empty()) out.push_back(s);
}

inline void StringSplit(std::wstring s, std::wstring delimiter, std::vector<std::wstring>& out)
{
	out.clear();
	size_t pos = 0;
	std::wstring token;
	while ((pos = s.find(delimiter)) != std::wstring::npos) {
		token = s.substr(0, pos);
		if (!token.empty())
			out.push_back(token);
		s.erase(0, pos + delimiter.length());
	}
	if(!s.empty()) out.push_back(s);
}

inline bool IsBlank(const std::wstring& s)
{
	bool blank = false;
	if (s.empty() || std::all_of(s.begin(), s.end(), [](char c) {return std::isspace(c); }))
	{
		blank = true;
	}
	return blank;
}

inline bool IsBlank(const std::string& s)
{
	bool blank = false;
	if (s.empty() || std::all_of(s.begin(), s.end(), [](char c) {return std::isspace(c); }))
	{
		blank = true;
	}
	return blank;
}

inline bool IsNumber(const std::string& s)
{
	std::string::const_iterator it = s.begin();
	if (std::isdigit(*it)) return false;
	int count = 0;
	while (it != s.end())
	{
		if (!std::isdigit(*it))
		{
			if (*it == '.')
			{
				++count;
				if (count == 2) return false;
			}
			else
			{
				return false;
			}
		}
		++it;
	}
	return true;
}


inline bool IsNumber(const std::wstring& s)
{
	std::wstring::const_iterator it = s.begin();
	if (!std::isdigit(*it)) return false;
	int count = 0;
	while (it != s.end())
	{
		if (!std::isdigit(*it))
		{
			if (*it == '.')
			{
				++count;
				if (count == 2) return false;
			}
			else
			{
				return false;
			}
		}
		++it;
	}
	return true;
}

inline const char* GetFilterDesc(const char* list[], int listSize)
{

}

inline static char __hex[256] = {};

inline std::string HexStrToStr(const std::string& hexStr)
{
	if (hexStr.length() % 2 != 0)
	{
		assert(0);
	}

	std::string re;

	for (size_t i = 0; i < hexStr.length(); i += 2)
	{
		re.push_back(::__hex[hexStr[i]] * 16 + ::__hex[hexStr[i + 1]]);
	}

	return re;
}


inline int RandomInt(int max, int min)
{
	srand(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	return min + rand() % (max + 1 - min);
}

inline std::string RandomString(int length = 10)
{
	std::string re;
	re.resize(length);
	for (size_t i = 0; i < length; i++)
	{
		re[i] = RandomInt(0, 127);
	}
	return re;
}

inline std::wstring RandomWString(int length = 10)
{
	std::wstring re;
	re.resize(length);
	for (size_t i = 0; i < length; i++)
	{
		re[i] = RandomInt(0, WCHAR_MAX);
	}
	return re;
}



#define _Params std::vector<std::wstring>
//example
//in_str = "string" "file.txt" "22" "3.14"
//=> out = { "string", "file.txt", "22", "3.14" }
inline void ExtractParams(std::wstring& in_str, std::vector<std::wstring>& out, size_t num = 1)
{
	StringSplit(in_str, L"\"", out);

	if (out.size() < num)
	{
		out.clear();
		StringSplit(in_str, L" ", out);
	}

	for (size_t i = 0; i < out.size(); i++)
	{
		std::wstring& cur = out[i];
		if (IsBlank(cur))
		{
			out.erase(out.begin() + i);
			i--;
		}
		else if (cur.back() == L'\\')
		{
			cur.pop_back();
		}
	}

}

inline std::string InputPassword()
{
#ifdef _DEBUG
	return "1234567890";
#else
	std::string pass = "";
	char ch;
	std::cout << "Password: ";
	ch = 0;

	while (true)
	{
		ch = _getch();

		if (ch == 13) break; //13 is Enter

		if (ch == 8) //8 is Backspace
		{
			if (!pass.empty())
			{
				pass.pop_back();
				std::cout << "\b \b";
			}
		}
		else
		{
			pass.push_back(ch);
			std::wcout << L'•';
		}

	}

	std::cout << '\n';

	return pass;
#endif // DEBUG
}

inline bool Confirm(const std::wstring& msg)
{
	std::wstring ch;
	std::wcout << msg << L" (y/n): ";
	std::getline(std::wcin, ch);
	ToLower(ch);
	if (L"y" == ch || L"yes" == ch)
	{
		return 1;
	}
	return 0;
}

inline int InitConsole()
{
	auto lo = std::wcout.imbue(std::locale("en_US.utf8"));
	lo = std::wcin.imbue(std::locale("en_US.utf8"));
#ifdef _WIN32
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001);
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		return GetLastError();
	}

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
	{
		return GetLastError();
	}

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hOut, dwMode))
	{
		return GetLastError();
	}

#endif
	return 1;
}

//return 1 on lock occupied
//return 0 in ow
inline int WaitToLock(std::mutex& locker, unsigned long perCheckTime = 16, long long timeout = LLONG_MAX)
{
	long long count = 0;
	while (!locker.try_lock())
	{
		count += perCheckTime;
		Sleep(perCheckTime);
		if (count > timeout)
		{
			return 0;
		}
	}

	return 1;
}

inline void Release(std::mutex& locker)
{
	locker.unlock();
}

inline bool IsReady(SOCKET sock, long interval)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = interval;

	return (select(0, &fds, &fds, 0, &tv) == 1);
}

inline bool IsReadyRead(SOCKET sock, long sec, long usec)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = usec;

	return (select(0, &fds, 0, 0, &tv) == 1);
}

inline bool IsReadyWrite(SOCKET sock, long sec, long usec)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = usec;

	return (select(0, 0, &fds, 0, &tv) == 1);
}

inline unsigned long ConsoleGetLine(wchar_t* buffer, size_t bufferSize, void* con = GetStdHandle(STD_INPUT_HANDLE))
{
	unsigned long read = 0;
	ReadConsole(con, &buffer[0], bufferSize, &read, NULL);
	buffer[read - 1] = 0;
	buffer[read - 2] = 0;
	return read;
}

//fake to_string (forgive me T_T)
namespace std
{
	inline string to_string(const char* p)
	{
		return string(p);
	}

	inline string to_string(const wchar_t* p)
	{
		return WStringToString(p);
	}

	inline wstring to_wstring(const wchar_t* p)
	{
		return wstring(p);
	}

	inline string to_string(const string& p)
	{
		return string(p);
	}

	inline wstring to_wstring(const wstring& p)
	{
		return wstring(p);
	}
};