#pragma once
#include <fstream>
#include <set>

#include <openssl/ossl_typ.h>

template <typename T> class Folder;
template <typename T> class File;

namespace FolderToSingleFileEncryption
{

//return num bytes written to fout
//if return value != freeSpaceSize, need next block to encrypt data
size_t Encrypt(std::fstream& fin, std::fstream& fout, 
	size_t freeSpaceSize,
	std::string& key, std::string IV, EVP_CIPHER_CTX* ctx,
	size_t thres = 150'000'000, size_t blockSize = 16 * 1024 * 1024 * 3);

//return offset to next block, 0 if end of data
size_t Decrypt(std::fstream& fin, std::fstream& fout, std::string& key,
	EVP_CIPHER_CTX* ctx,
	size_t thres = 150'000'000, size_t blockSize = 16 * 1024 * 1024 * 3);


class EncryptedFolder;


//do encrypt folder and all content to only one file
EncryptedFolder* EncryptFolder(Folder<size_t>** pfolder, const std::wstring& out_path, std::string key, std::string IV = "");

//open an encrypted folder
EncryptedFolder* OpenEncryptedFolder(const std::wstring& encrpytedFolderPath, std::string key);

//return the decrypted folder
Folder<size_t>* DecryptFolder(EncryptedFolder* encryptedFolder, const std::wstring& out_path, std::string key);


struct _FreeSpace
{
	size_t offset = 0;
	size_t size = 0;
};

#ifdef DecryptFile
#undef DecryptFile
#endif // DecryptFile


#define _EXTRA_DATA 48
//all offset is offset from begin of file
//struct of 1 free block is:
/*
	|8 bytes : IV size| |IV| |8 bytes : size of Data| |Data| |8 bytes : offset to next block, 0 if end of Data|
*/
class EncryptedFolder
{
private:
	EVP_CIPHER_CTX* m_encryptor = nullptr;
	EVP_CIPHER_CTX* m_decryptor = nullptr;

	size_t m_curOffset = 0;

	std::string m_keyVerifyString;

public:
	Folder<size_t>* m_root = nullptr;

	//list free space block (offset)
	std::set<_FreeSpace, bool(*)(const _FreeSpace&, const _FreeSpace&)> m_freeSpace;

	using FreeSpaceIter = std::set<_FreeSpace, bool(*)(const _FreeSpace&, const _FreeSpace&)>::iterator;

	//out put stream to encrypted folder
	std::fstream* m_fstream = nullptr;

	//path to EncryptedFolder
	std::wstring m_path;

	std::wstring m_name;

	bool m_changed = false;

public:
	inline ~EncryptedFolder();

public:
	friend EncryptedFolder* EncryptFolder(Folder<size_t>**, const std::wstring&, std::string, std::string);
	friend EncryptedFolder* OpenEncryptedFolder(const std::wstring&, std::string);
	friend Folder<size_t>* DecryptFolder(EncryptedFolder* encryptedFolder, const std::wstring& out_path, std::string key);;
	
public:
	bool VerifyKey(std::string& key);

private:
	void Save(std::string key, std::fstream& fout);

	_FreeSpace ReadFreeBlock(size_t offset);

	//bool is mergeable
	std::pair<_FreeSpace, bool> Merge(const _FreeSpace& b1, const _FreeSpace& b2);

	

public:
	//encrypt the master record
	void Save(std::string key);

	void Close();

private:
	void AddFile(File<size_t>* file, std::fstream& fin, std::string key);

	void AddFolder(Folder<size_t>* parent, Folder<size_t>* newFolder, 
		bool(*callback)(File<size_t>*, Folder<size_t>*), std::string& key);

	void Remove(File<size_t>* file);

public:
	Folder<size_t>* FindFolder(const std::wstring& destination);
	File<size_t>* FindFile(const std::wstring& destination);


public:
	//add file to encrypted folder

	//throw if file exists.
	void AddNewFile(const std::wstring& from, const std::wstring& to, std::string key);
	//replace if file exists.
	void AddFileReplaceExists(const std::wstring& from, const std::wstring& to, std::string key);

	//throw if folder exists.
	void AddNewFolder(const std::wstring& from, const std::wstring& to, std::string key);
	//if file in source folder exists in destination folder, call callback()
	//if callback() return true, replace file in destination folder
	//else do nothing
	void AddFolder(const std::wstring& from, const std::wstring& to, std::string key, 
		bool(*callback)(File<size_t>*, Folder<size_t>*));


	bool RemoveFile(const std::wstring& path);
	bool RemoveFolder(const std::wstring& path);

public:
	size_t GetFileSize(File<size_t>* file);

	//include IV, iv size, data size, data, next block offset.
	size_t GetSize(File<size_t>* file);

	//read all block of file to buffer
	//if not enough bufferSize return current pos of stream
	void Read(File<size_t>* file, char* buffer);

	void Read(size_t& out_curPos, size_t& out_remainSize, char* buffer, size_t bufferSize);

public:
	//decrypt file to fstream (in storage, not in encrypted folder)
	void DecryptFile(File<size_t>* file, std::fstream& fstream, std::string& key);

	void DecryptFolder(Folder<size_t>* folder, const std::wstring& to, std::string& key);

public:
	void DecryptFile(const std::wstring& from, std::fstream& fstream, std::string& key);
	void DecryptFile(const std::wstring& from, const std::wstring& to, std::string key);
	void DecryptFolder(const std::wstring& from, const std::wstring& to, std::string key);

public:
	inline bool Changed() { return m_changed; };

};

inline EncryptedFolder::~EncryptedFolder()
{
	Close();
}



}
