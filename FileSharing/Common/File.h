#pragma once
#include <string>
#include <algorithm>
#include <shlwapi.h>
#include <fstream>
#include "CommonFuction.h"

#pragma comment(lib, "Shlwapi.lib")

inline void StandardPath(std::wstring& path)
{
	std::replace(path.begin(), path.end(), '/', '\\');
}

inline std::wstring CombinePath(std::wstring curDir, std::wstring relativePath)
{
	if (IsBlank(relativePath)) return curDir;
	StandardPath(relativePath);
	std::wstring begin = relativePath.substr(0, 3);

	int count = 0;
	if (begin[0] == '.' && begin[1] == '\\')
	{
		relativePath.erase(relativePath.begin(), relativePath.begin() + 2);
	}
	else
	{
		while (begin == L"..\\")
		{
			count++;
			relativePath.erase(relativePath.begin(), relativePath.begin() + 3);
			begin = relativePath.substr(0, 3);
		}
	}
	

	if (count == 0)
	{
		if (relativePath.find(L":\\") != std::wstring::npos)
		{
			return relativePath;
		}
		else 
		{
			while (*(relativePath.begin()) == L'\\')
			{
				relativePath.erase(relativePath.begin());
			}
		}
	}

	for (size_t i = 0; i < count; i++)
	{
		if (curDir.back() == L':') break;
		size_t index = curDir.find_last_of('\\');
		curDir.erase(curDir.begin() + index, curDir.end());
	}

	return curDir.back() == '\\' ? curDir + relativePath : curDir + L"\\" + relativePath;

}

inline std::wstring MakeRelativePath(std::wstring from, std::wstring to)
{
	wchar_t buffer[MAX_PATH] = L"";

	std::replace(from.begin(), from.end(), '/', '\\');
	std::replace(to.begin(), to.end(), '/', '\\');

	PathRelativePathToW(buffer, from.c_str(), FILE_ATTRIBUTE_DIRECTORY, to.c_str(), GetFileAttributesW(to.c_str()));

	return buffer;
}

inline std::wstring GetExtension(std::wstring path)
{
	auto a = [](unsigned char c) { return std::tolower(c); };
	std::wstring ext = path.substr(path.find_last_of(L'.') + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), a);
	return ext;
}

inline std::wstring GetFileName(std::wstring path)
{
	std::wstring fileName = path.substr(path.find_last_of(L"\\/") + 1);
	if (fileName.empty()) return path;
	return fileName;
}

inline std::string GetFileNameA(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	std::string fileName = path.substr(path.find_last_of('/') + 1);
	if (fileName.empty()) fileName = path.substr(path.find_last_of('\\') + 1);
	return fileName;
}

inline bool FileExist(const std::wstring path)
{
	std::ifstream f(path.c_str(), std::ios::binary);
	return f.good();
}


#define FILE_COPY_REPLACE_EXIST 1

//if file exist, make a new one has name <name>_<index>
#define FILE_COPY_KEEP_BOTH 2

//if file exist, return FILE_COPY_RETURN_DEST_FILE_EXIST
#define FILE_COPY_DONT_REPLACE_EXIST 3


#define FILE_COPY_RETURN_SUCCESS 1
#define FILE_COPY_RETURN_DEST_FILE_EXIST 2
#define FILE_COPY_RETURN_SRC_FILE_NOT_EXIST 3
#define FILE_COPY_RETURN_MODE_INVALID 4

//access write error, file is still being use by another process,...
#define FILE_COPY_RETURN_UNDEFINED_REASON 5

//mode is FILE_COPY_...
//return FILE_COPY_RETURN_...
inline int FileCopy(std::wstring srcPath, std::wstring destPath, int mode)
{
	if (!FileExist(srcPath)) return FILE_COPY_RETURN_SRC_FILE_NOT_EXIST;
	switch (mode)
	{
	case FILE_COPY_REPLACE_EXIST:

		if (CopyFile(srcPath.c_str(), destPath.c_str(), false) == 0) 
		{
			return FILE_COPY_RETURN_UNDEFINED_REASON;
		};

		break;
	case FILE_COPY_KEEP_BOTH:

		if (FileExist(destPath))
		{
			std::wstring ext = GetExtension(destPath);
			std::wstring prePath = destPath.substr(0, destPath.length() - ext.length() - 1);

			int count = 1;
			while (FileExist(destPath))
			{
				destPath = prePath + L"_" + std::to_wstring(count) + L"." + ext;
				count++;
			}
		}

		if (CopyFile(srcPath.c_str(), destPath.c_str(), false) == 0)
		{
			return FILE_COPY_RETURN_UNDEFINED_REASON;
		};

		break;
	case FILE_COPY_DONT_REPLACE_EXIST:

		if (FileExist(destPath))
		{
			return FILE_COPY_RETURN_DEST_FILE_EXIST;
		}

		if (CopyFile(srcPath.c_str(), destPath.c_str(), false) == 0)
		{
			return FILE_COPY_RETURN_UNDEFINED_REASON;
		};

		break;
	default:

		return FILE_COPY_RETURN_MODE_INVALID;

		break;
	}

	return FILE_COPY_RETURN_SUCCESS;
}

struct FileTime
{
	_SYSTEMTIME createTime;
	_SYSTEMTIME lastAccessTime;
	_SYSTEMTIME lastWriteTime;
};

inline FileTime GetFileTime(std::wstring path)
{
	auto fHandle = CreateFile(path.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (fHandle == NULL) return {};

	_FILETIME create;
	_FILETIME lastAccess;
	_FILETIME lastWrite;
	GetFileTime(fHandle, &create, &lastAccess, &lastWrite);

	FileTime re;

	_SYSTEMTIME createTime;
	FileTimeToSystemTime(&create, &createTime);
	SystemTimeToTzSpecificLocalTime(0, &createTime, &re.createTime);

	_SYSTEMTIME lastAccessTime;
	FileTimeToSystemTime(&lastAccess, &lastAccessTime);
	SystemTimeToTzSpecificLocalTime(0, &lastAccessTime, &re.lastAccessTime);

	_SYSTEMTIME lastWriteTime;
	FileTimeToSystemTime(&lastWrite, &lastWriteTime);
	SystemTimeToTzSpecificLocalTime(0, &lastWriteTime, &re.lastWriteTime);

	CloseHandle(fHandle);

	return re;
}

inline size_t GetFileSize(const std::wstring& filename)
{
	std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
	if (in.bad()) throw "File not found !!!";
	return in.tellg();
}

inline void CreateNewDirectory(const std::wstring& path)
{
	if (!(CreateDirectory(path.c_str(), NULL) ||
		ERROR_ALREADY_EXISTS == GetLastError()))
	{
		Throw(L"CreateNewDirectory() error.");
	}
}

//valid only, not check exists.
inline bool IsValidRelativePath(const std::wstring& path)
{
#ifdef _WIN32

	if (path.find(L':') != std::wstring::npos) return false;

	return true;

#endif // _WIN32
}

inline void SetCurrentDir(const std::wstring& path)
{
#ifdef _WIN32
	SetCurrentDirectory(path.c_str());
#endif
}

//path to folder hold temp file
inline std::wstring GenerateTempFilePath(std::wstring path)
{
#ifdef _WIN32
	unsigned long long time = GetTickCount64();

	if (path.back() != L'\\') path.push_back(L'\\');

	auto re = path + std::to_wstring(time);

	while (FileExist(re))
	{
		time++;
		re = path + std::to_wstring(time);
	}

	return re;

#else
	Throw(L"Not Windows platform.")
#endif

}