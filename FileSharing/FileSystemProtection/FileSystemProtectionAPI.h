#pragma once
#include <fstream>

#include <openssl/ossl_typ.h>

#include "FolderToSingleFileEncryption.h"

template <typename T> class Folder;
template <typename T> class File;

#ifdef EncryptFile
#undef EncryptFile
#endif

#ifdef DecryptFile
#undef DecryptFile
#endif

void EncryptFile(const std::wstring& in_path, const std::wstring& out_path, 
	std::string key, std::string IV, EVP_CIPHER_CTX* ctx = nullptr,
	size_t thres = 150'000'000, size_t blockSize = 16 * 1024 * 1024 * 3);

//return num bytes written to fout
size_t EncryptFile(std::ifstream& fin, std::ofstream& fout,
	std::string key, std::string IV, EVP_CIPHER_CTX* ctx = nullptr,
	size_t thres = 150'000'000, size_t blockSize = 16 * 1024 * 1024 * 3);

//return num bytes written to fout
size_t DecryptFile(std::ifstream& fin, std::ofstream& fout,
	std::string key, EVP_CIPHER_CTX* ctx = nullptr,
	size_t thres = 150'000'000, size_t blockSize = 16 * 1024 * 1024 * 3);

//return num bytes written to fout
size_t DecryptFile(std::ifstream& fin, std::ofstream& fout, size_t fileSize,
	std::string key, EVP_CIPHER_CTX* ctx = nullptr,
	size_t thres = 150'000'000, size_t blockSize = 16 * 1024 * 1024 * 3);


//==============================================================================================================================


//do encrypt folder and all content to a new folder
void EncryptFolder(Folder<std::string>* folder, const std::wstring& out_path, std::string key, std::string IV = "");

//data of virtual files in virtual folder is name to encrypted file
//open an encrypted folder that encrypted by EncryptFolder()
Folder<std::string>* OpenEncryptedFolder(const std::wstring& encrpytedFolderPath, std::string key);

//in_path is location of the encrypted folder in storage
//return the name of folder that decrypted
std::wstring DecryptFolder(const std::wstring& in_path, const std::wstring& out_path, std::string key);


//==============================================================================================================================


struct _ExtraData
{
	//offset from begining of file
	size_t offset = 0;

	//size of encrypted file
	size_t size = 0;
};

//removed
void EncryptFolderToFile(Folder<_ExtraData>* folder, const std::wstring& out_path, std::string key, std::string IV = "");
Folder<_ExtraData>* OpenEncryptedFolderFromFile(const std::wstring& encrpytedFolderPath, std::string key);
std::wstring DecryptFolderFromFile(const std::wstring& in_path, const std::wstring& out_path, std::string key);