#pragma once
#include <fstream>
#include <string>

#include <openssl\evp.h>

class TempFile
{
public:
	std::fstream m_fstream;
	std::wstring m_path;

public:
	//path to folder hold temp file
	inline TempFile(const std::wstring& path)
	{
		m_path = GenerateTempFilePath(path);
		m_fstream.open(path, std::ios::binary | std::ios::out);

		int attr = GetFileAttributes(m_path.c_str());
		SetFileAttributes(m_path.c_str(),
			attr | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY);
	};

	inline std::fstream& ReadMode()
	{
		m_fstream.flush();
		m_fstream.close();
		m_fstream.open(m_path, std::ios::binary | std::ios::in);
		return m_fstream;
	};

	inline std::fstream& WriteMode()
	{
		m_fstream.close();
		m_fstream.open(m_path, std::ios::binary | std::ios::out);
		return m_fstream;
	}

	inline ~TempFile()
	{
		m_fstream.close();
		DeleteFile(m_path.c_str());
	};


};

#ifdef EncryptFile
#undef EncryptFile
#endif // EncryptFile


inline void EncryptFile(std::istream& fin, std::ostream& fout, std::string password,
	size_t thres = 150'000'000, size_t blockSize = 4 * 1024 * 1024)
{
	password.resize(32);

	auto iv = RandomString(32);

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

	long long fileSize = fin.seekg(0, fin.end).tellg();
	fin.seekg(0, fin.beg);

	if (fileSize > thres)
	{
		//per block encrypt
	}
	else
	{
		char* buffer = new char[fileSize + 1]();
		char* outBuffer = new char[fileSize + 16 + 1]();

		size_t numBytes = 0;

		size_t ivSize = 32;
		size_t dataSize = 0;

		int outlen = 0;
		int pad = 0;

		fout.write((char*)&ivSize, 8);
		fout.write((char*)&iv[0], ivSize);
		fout.write((char*)&fileSize, 8);

		fin.read(buffer, fileSize);

		EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)password.c_str(), (uint8_t*)&iv[0]);

		EVP_EncryptUpdate(ctx, (uint8_t*)outBuffer, &outlen, (uint8_t*)buffer, fileSize);

		EVP_EncryptFinal(ctx, (uint8_t*)outBuffer + outlen, &pad);

		fout.write(outBuffer, outlen + pad);

		delete[] buffer, outBuffer;
	}

	EVP_CIPHER_CTX_free(ctx);
}

inline void DecryptFile(std::istream& fin, std::ostream& fout, std::string password,
	size_t thres = 150'000'000, size_t blockSize = 4 * 1024 * 1024)
{
	password.resize(32);

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

	long long fileSize = fin.seekg(0, fin.end).tellg();
	fin.seekg(0, fin.beg);

	if (fileSize > thres)
	{
		//per block decrypt
	}
	else
	{
		char* buffer = new char[fileSize + 1]();
		char* outBuffer = new char[fileSize + 1]();

		size_t numBytes = 0;

		char iv[33] = {};
		size_t ivSize = 0;
		size_t dataSize = 0;

		int outlen = 0;
		int pad = 0;

		while (fileSize > 0)
		{
			fin.read((char*)&ivSize, 8);
			fin.read((char*)&iv, ivSize);
			fin.read((char*)&dataSize, 8);

			fin.read(buffer, dataSize);

			EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)password.c_str(), (uint8_t*)iv);

			EVP_DecryptUpdate(ctx, (uint8_t*)outBuffer, &outlen, (uint8_t*)buffer, dataSize);

			EVP_DecryptFinal(ctx, (uint8_t*)outBuffer + outlen, &pad);

			fout.write(outBuffer, outlen + pad);

			fileSize -= (24 + ivSize + dataSize);

			fin.read((char*)&dataSize, 8);
		}

		delete[] buffer, outBuffer;
	}

	EVP_CIPHER_CTX_free(ctx);
}