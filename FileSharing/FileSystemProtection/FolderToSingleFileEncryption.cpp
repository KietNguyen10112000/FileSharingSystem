#include "FolderToSingleFileEncryption.h"

#include <sstream>

#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/sha.h>

#include "FileFolder.h"

#include "../Common/File.h"

#ifdef EncryptFile
#undef EncryptFile
#endif

#ifdef DecryptFile
#undef DecryptFile
#endif

#define VERIFY_STR_LENGTH 64

namespace FolderToSingleFileEncryption
{

size_t Encrypt(std::fstream& fin, std::fstream& fout, size_t freeSpaceSize, std::string& key,
	std::string IV, EVP_CIPHER_CTX* ctx, size_t thres, size_t blockSize)
{
	EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());

	size_t curOffset = fin.tellg();

	fin.seekg(0, fin.end);
	size_t fileSize = fin.tellg();
	fin.seekg(curOffset, fin.beg);

	//cipherLen = (clearLen/16 + 1) * 16; //for CBC, CTR mode
	//write len of cipher text
	size_t cipherSize = fileSize;

	size_t IVsize = IV.size();
	fout.write((char*)(&IVsize), 8);
	fout.write(IV.c_str(), IVsize);

	cipherSize += 8 + 8 + IV.size();

	fileSize = min(freeSpaceSize, fileSize);

	fout.write((char*)&fileSize, 8);

	if (fileSize < thres)
	{
		char* buffer = new char[fileSize];

		char* cipherBuffer = new char[fileSize];

		int outSize;

		fin.read(buffer, fileSize);

		EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, fileSize);

		int foutSize;

		EVP_EncryptFinal_ex(ctx, (uint8_t*)(cipherBuffer + outSize), &foutSize);

		fout.write((const char*)cipherBuffer, foutSize + outSize);

		delete[] cipherBuffer;
		delete[] buffer;
	}
	else
	{
		char* buffer = new char[blockSize];
		char* cipherBuffer = new char[blockSize + AES_BLOCK_SIZE];

		long long numChar = 0;

		int outSize;

		while (fileSize > blockSize)
		{
			numChar = fin.read(buffer, blockSize).gcount();

			EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, blockSize);

			fout.write(cipherBuffer, outSize);
			fileSize -= outSize;
		}

		numChar = fin.read(buffer, fileSize).gcount();

		EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, numChar);

		int foutSize;

		EVP_EncryptFinal_ex(ctx, (uint8_t*)(cipherBuffer + outSize), &foutSize);

		fout.write(cipherBuffer, outSize + foutSize);

		assert(fileSize <= outSize + foutSize);

		delete[] cipherBuffer;
		delete[] buffer;

	}

	//fileSize = 0;

	//fout.write((char*)&fileSize, 8);

	return cipherSize;
}

size_t Decrypt(std::fstream& fin, std::fstream& fout, std::string& key, 
	EVP_CIPHER_CTX* ctx, size_t thres, size_t blockSize)
{
	size_t ivSize = 0;
	fin.read((char*)&ivSize, 8);
	char iv[33] = {};
	fin.read(iv, ivSize);

	size_t cipherTextSize = 0;
	fin.read((char*)&cipherTextSize, 8);

	EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)iv);

	char* cipherText = nullptr;
	char* plainText = nullptr;

	size_t plainTextSize = 0;

	if (cipherTextSize < thres)
	{
		cipherText = new char[cipherTextSize]();
		plainText = new char[cipherTextSize]();

		fin.read(cipherText, cipherTextSize);

		EVP_DecryptUpdate(ctx, (uint8_t*)plainText, (int*)&plainTextSize, (uint8_t*)cipherText, cipherTextSize);

		int pad = 0;

		EVP_DecryptFinal_ex(ctx, (uint8_t*)(plainText + plainTextSize), &pad);

		fout.write(plainText, plainTextSize);

	}
	else
	{
		cipherText = new char[blockSize]();
		plainText = new char[blockSize]();

		while (blockSize < cipherTextSize)
		{
			fin.read(cipherText, blockSize);
			cipherTextSize -= blockSize;

			EVP_DecryptUpdate(ctx, (uint8_t*)plainText, (int*)&plainTextSize, (uint8_t*)cipherText, blockSize);
			fout.write(plainText, blockSize);

		}

		fin.read(cipherText, cipherTextSize);
		EVP_DecryptUpdate(ctx, (uint8_t*)plainText, (int*)&plainTextSize, (uint8_t*)cipherText, cipherTextSize);

		int pad = 0;

		EVP_DecryptFinal_ex(ctx, (uint8_t*)(plainText + plainTextSize), &pad);

		fout.write(plainText, plainTextSize + pad);
	}

	delete[] cipherText;
	delete[] plainText;

	fin.read((char*)&cipherTextSize, 8);

	return cipherTextSize;
}

EncryptedFolder* EncryptFolder(Folder<size_t>** pfolder, const std::wstring& out_path, std::string key, std::string IV)
{
	EncryptedFolder* returnVal = new EncryptedFolder();

	returnVal->m_root = *pfolder;
	returnVal->m_path = out_path;
	returnVal->m_encryptor = EVP_CIPHER_CTX_new();

	auto& folder = returnVal->m_root;
	std::fstream fout(out_path, std::ios::binary | std::ios::out);

	key.resize(32);

	EVP_CIPHER_CTX* e_ctx = returnVal->m_encryptor;
	EVP_CIPHER_CTX_init(e_ctx);

	//offset to master record
	fout.write("12345678", 8);

	size_t& count = returnVal->m_curOffset;

	count = 8;

	size_t temp = 0;

	folder->Traverse(
		[&](Folder<size_t>* current)
		{
			for (auto& f : current->Files())
			{
				f->Data() = count;

				std::fstream fin(f->Path(), std::ios::binary | std::ios::in);

				std::string iv = RandomString(32);
				
				auto size = Encrypt(fin, fout, ULLONG_MAX, key, { iv }, e_ctx);

				fout.write((char*)&temp, 8);

				count += size + 8;

				fin.close();
			}
		},
		nullptr);

	returnVal->m_freeSpace.insert({ count, ULLONG_MAX });

	returnVal->Save(key, fout);

	fout.close();

	returnVal->m_fstream = new std::fstream(out_path, std::ios::binary | std::ios::in | std::ios::out);

	*pfolder = nullptr;

	return returnVal;
}

std::string GenerateKeyVerifyString(std::string key, size_t verifyLength, EVP_CIPHER_CTX* ctx)
{
	if (verifyLength < 64) Throw(L"verifyLength must > 64");

	std::string rand = RandomString(verifyLength);
	std::string IV = RandomString(32);
	key.resize(32);

	EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());

	char* cipherBuffer = new char[rand.size() + 1]();

	int outSize;

	EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)rand.c_str(), rand.size());

	int foutSize;

	EVP_EncryptFinal_ex(ctx, (uint8_t*)(cipherBuffer + outSize), &foutSize);

	std::string re;
	re.resize(verifyLength + foutSize + outSize + 32);


	memcpy(&re[0], IV.c_str(), 32);
	memcpy(&re[32], rand.c_str(), verifyLength);
	memcpy(&re[32 + verifyLength], cipherBuffer, foutSize + outSize);

	delete[] cipherBuffer;

	return { re };
}

bool __FreeSpaceCmp(const _FreeSpace& b1, const _FreeSpace& b2)
{
	return b1.offset < b2.offset;
}

EncryptedFolder* OpenEncryptedFolder(const std::wstring& encrpytedFolderPath, std::string key)
{
	EncryptedFolder* returnVal = new EncryptedFolder();

	returnVal->m_path = encrpytedFolderPath;
	returnVal->m_name = GetFileName(encrpytedFolderPath);
	returnVal->m_decryptor = EVP_CIPHER_CTX_new();
	returnVal->m_encryptor = EVP_CIPHER_CTX_new();

	auto& d_ctx = returnVal->m_decryptor;

	EVP_CIPHER_CTX_init(d_ctx);

	auto& folder = returnVal->m_root;

	key.resize(32);

	std::ifstream fin(encrpytedFolderPath, std::ios::binary | std::ios::in);

	size_t& offset = returnVal->m_curOffset;
	fin.read((char*)&offset, 8);

	fin.seekg(offset, fin.beg);

	char IVBuff[33] = {};


	//======================================================================================================
	//read free space

	fin.read(IVBuff, 32);

	size_t freeSize = 0;
	fin.read((char*)&freeSize, 8);

	freeSize = freeSize *sizeof(_FreeSpace);

	char* cipherFreeBuffer = new char[freeSize + 1]();
	char* plainFreeBuffer = new char[freeSize + 1]();

	size_t plainFreeBufferSize = 0;

	fin.read(cipherFreeBuffer, freeSize);

	EVP_DecryptInit_ex(d_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);
	
	EVP_DecryptUpdate(d_ctx, (uint8_t*)plainFreeBuffer, (int*)&plainFreeBufferSize, (uint8_t*)cipherFreeBuffer, freeSize);

	returnVal->m_freeSpace 
		= std::set<_FreeSpace, bool(*)(const _FreeSpace&, const _FreeSpace&)>((
			_FreeSpace*)plainFreeBuffer, (_FreeSpace*)(plainFreeBuffer + freeSize), __FreeSpaceCmp);

	delete[] cipherFreeBuffer, plainFreeBuffer;


	//======================================================================================================
	//read master record
	
	fin.read(IVBuff, 32);

	size_t materRecordSize = 0;
	fin.read((char*)&materRecordSize, 8);

	char* materRecordContent = new char[materRecordSize];

	fin.read(materRecordContent, materRecordSize);

	char* plainText = new char[materRecordSize + AES_BLOCK_SIZE]();
	int plainTextSize;

	EVP_DecryptInit_ex(d_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);

	EVP_DecryptUpdate(d_ctx, (uint8_t*)plainText, &plainTextSize, (uint8_t*)materRecordContent, materRecordSize);

	int fSize;

	EVP_DecryptFinal_ex(d_ctx, (uint8_t*)(plainText + plainTextSize), &fSize);

	std::wstring content = std::wstring((const wchar_t*)plainText, (const wchar_t*)(plainText + plainTextSize));

	delete[] materRecordContent;
	delete[] plainText;

	std::wstringstream ss(content);

	returnVal->m_root = new Folder<size_t>();
	auto& root = returnVal->m_root;

	try
	{
		root->LoadStructure(ss,
			[](std::wistream& win, File<size_t>* file)
			{
				auto& data = file->Data();
				win.read((wchar_t*)&data, sizeof(data) / sizeof(wchar_t));
				std::wstring line;
				std::getline(win, line);

			});
	}
	catch (...)
	{
		SafeDelete(returnVal);
		throw;
	}

	fin.close();

	returnVal->m_fstream = new std::fstream(encrpytedFolderPath, std::ios::binary | std::ios::in | std::ios::out);
	returnVal->m_fstream->seekg(offset, returnVal->m_fstream->beg);

	returnVal->m_keyVerifyString = GenerateKeyVerifyString(key, VERIFY_STR_LENGTH, returnVal->m_encryptor);

	returnVal->m_keyVerifyString.resize(returnVal->m_keyVerifyString.size() + VERIFY_STR_LENGTH);

	return returnVal;
}

Folder<size_t>* DecryptFolder(EncryptedFolder* encryptedFolder, const std::wstring& out_path, std::string key)
{
	auto folder = encryptedFolder->m_root;

	key.resize(32);

	std::fstream fin(encryptedFolder->m_path, std::ios::binary | std::ios::in);

	folder->Traverse(
		[&](Folder<size_t>* current)
		{
			CreateNewDirectory(CombinePath(out_path, current->Path()));
		},
		nullptr
	);

	folder->Traverse(
		[&](Folder<size_t>* current)
		{
			for (auto& f : current->Files())
			{
				auto outpath = CombinePath(out_path, f->Path());
				std::fstream fout(outpath, std::ios::binary | std::ios::out);

				fin.seekg(f->Data(), fin.beg);
				size_t nextOffset = Decrypt(fin, fout, key, encryptedFolder->m_decryptor);

				while (nextOffset != 0)
				{
					fin.seekg(nextOffset, fin.beg);
					nextOffset = Decrypt(fin, fout, key, encryptedFolder->m_decryptor);
				}

				fout.close();
			}
		},
		nullptr
	);

	fin.close();

	folder->MoveTo(out_path);

	encryptedFolder->m_root = nullptr;

	return folder;
}

bool EncryptedFolder::VerifyKey(std::string& key)
{
	key.resize(32);

	char* IV = &m_keyVerifyString[0];
	char* rand = &m_keyVerifyString[32];
	char* cipher = &m_keyVerifyString[32 + VERIFY_STR_LENGTH];

	char* cipherBuff = &m_keyVerifyString[32 + 2 * VERIFY_STR_LENGTH];

	EVP_EncryptInit_ex(m_encryptor, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV);

	int outSize;

	EVP_EncryptUpdate(m_encryptor, (uint8_t*)cipherBuff, &outSize, (uint8_t*)rand, VERIFY_STR_LENGTH);

	int foutSize;

	EVP_EncryptFinal_ex(m_encryptor, (uint8_t*)(cipherBuff + outSize), &foutSize);

	if (memcmp(cipher, cipherBuff, VERIFY_STR_LENGTH) == 0)
	{
		return true;
	}

	return false;
}

void EncryptedFolder::Save(std::string key, std::fstream& fout)
{
	auto& folder = m_root;
	auto& e_ctx = m_encryptor;

	std::string IV = RandomString(32);
	fout.write(IV.c_str(), 32);

	//======================================================================================================
	//write free space
	auto freeSize = m_freeSpace.size();
	fout.write((char*)&freeSize, 8);

	size_t bufferLen = m_freeSpace.size() * sizeof(_FreeSpace);
	char* buffer = new char[bufferLen]();

	char* cipherBuffer = new char[bufferLen]();
	size_t cipherBufferSize = 0;

	size_t c = 0;
	for (auto i : m_freeSpace)
	{
		*((size_t*)&buffer[c]) = i.offset;
		*((size_t*)&buffer[c + 8]) = i.size;
		c += 16;
	}

	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());

	EVP_EncryptUpdate(e_ctx, (uint8_t*)cipherBuffer, (int*)&cipherBufferSize, (uint8_t*)buffer, bufferLen);

	int pad = 0;
	EVP_EncryptFinal_ex(e_ctx, (uint8_t*)(cipherBuffer + cipherBufferSize), &pad);

	fout.write((char*)cipherBuffer, cipherBufferSize + pad);

	delete[] buffer;
	delete[] cipherBuffer;

	//======================================================================================================
	//write master record

	IV = RandomString(32);
	fout.write(IV.c_str(), 32);

	folder->ToRelativePath();

	std::wstring masterRecord = folder->ToStructureString(
		[](std::wostream& out, File<size_t>* file)
		{
			auto& data = file->Data();
			out.write((wchar_t*)&data, sizeof(data) / sizeof(wchar_t));
		});

	size_t recordLen = masterRecord.length() * 2;

	//write out the ciphertext len
	fout.write((char*)&recordLen, 8);

	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());

	uint8_t* ciphertext = new uint8_t[recordLen]();
	int outSize = 0;

	EVP_EncryptUpdate(e_ctx, (uint8_t*)ciphertext, &outSize, (uint8_t*)masterRecord.c_str(), recordLen);

	int foutSize;

	EVP_EncryptFinal_ex(e_ctx, (uint8_t*)(ciphertext + outSize), &foutSize);

	fout.write((char*)ciphertext, outSize + foutSize);


	//======================================================================================================
	//jump to begin and write offset to encrypted structure

	fout.seekp(0, fout.beg);
	fout.write((char*)&m_curOffset, 8);

	delete[] ciphertext;
	m_changed = false;
}

_FreeSpace EncryptedFolder::ReadFreeBlock(size_t offset)
{
	size_t curOffset = m_fstream->tellg();

	char buffer[33];
	
	size_t size = 0;

	m_fstream->read((char*)&size, 8);

	m_fstream->read(buffer, size);

	m_fstream->read((char*)&size, 8);

	m_fstream->seekg(curOffset, m_fstream->beg);

	return { offset , size };
}

std::pair<_FreeSpace, bool> EncryptedFolder::Merge(const _FreeSpace& b1, const _FreeSpace& b2)
{
	std::pair<_FreeSpace, bool> re = { { 0,0 }, false };

	if (b1.offset < b2.offset)
	{
		if (b1.offset + _EXTRA_DATA + b1.size + 8 == b2.offset)
		{
			re.second = true;
			re.first.offset = b1.offset;
			re.first.size = b1.size + b2.size + _EXTRA_DATA + 8;
		}
	}
	else
	{
		if (b2.offset + _EXTRA_DATA + b2.size + 8 == b1.offset)
		{
			re.second = true;
			re.first.offset = b2.offset;
			re.first.size = b1.size + b2.size + _EXTRA_DATA + 8;
		}
	}
	

	return re;
}

//collect space only, not remove on master record
void EncryptedFolder::Remove(File<size_t>* file)
{
	if (!file) return;

	m_changed = true;

	size_t size = 0;
	size_t curOffset = file->Data();

	while (curOffset)
	{
		m_fstream->seekg(curOffset + (_EXTRA_DATA - 8), m_fstream->beg);

		m_fstream->read((char*)&size, 8);

		auto inserted = m_freeSpace.insert({ curOffset, size });

		auto cur = inserted.first;

		std::pair<_FreeSpace, bool> re;

		if (cur != m_freeSpace.begin())
		{
			auto pre = std::prev(cur);
			re = Merge(*pre, *cur);
			if (re.second)
			{
				m_freeSpace.erase(cur);
				m_freeSpace.erase(pre);
				inserted = m_freeSpace.insert(re.first);
			}

		}

		cur = inserted.first;
		auto next = std::next(inserted.first);
		if (next != m_freeSpace.end())
		{
			re = Merge(*cur, *next);
			if (re.second)
			{
				m_freeSpace.erase(cur);
				m_freeSpace.erase(next);
				m_freeSpace.insert(re.first);
			}
		}

		m_fstream->seekg(curOffset + _EXTRA_DATA + size, m_fstream->beg);

		m_fstream->read((char*)&curOffset, 8);
	}
	

}


void EncryptedFolder::Save(std::string key)
{
	if (!m_changed) return;
	m_fstream->seekp(m_curOffset, m_fstream->beg);
	if(key.size() != 32) key.resize(32);
	Save(key, *m_fstream);
}

void EncryptedFolder::Close()
{
	if(m_encryptor) EVP_CIPHER_CTX_free(m_encryptor);
	if (m_decryptor) EVP_CIPHER_CTX_free(m_decryptor);

	if (m_fstream)
	{
		m_fstream->close();
		delete m_fstream;
	}

	if (m_root)
	{
		m_root->RecursiveFree();
		delete m_root;
	}
}

void EncryptedFolder::AddFile(File<size_t>* file, std::fstream& fin, std::string key)
{
	if (!file) return;

	m_changed = true;

	if (key.size() != 32) key.resize(32);

	fin.seekg(0, fin.end);
	size_t fileSize = fin.tellg();
	fin.seekg(0, fin.beg);


	while (true)
	{
		FreeSpaceIter space = m_freeSpace.begin();
		std::string iv = RandomString(32);

		m_fstream->seekp(space->offset, fin.beg);
		size_t encryptedSize = Encrypt(fin, *m_fstream, space->size, key, { iv }, m_encryptor);
		fileSize -= (encryptedSize - _EXTRA_DATA);

		if (file->m_data == 0) file->m_data = space->offset;

		size_t size = space->size - encryptedSize - 8;
		size_t offset = space->offset + encryptedSize + 8;

		m_freeSpace.erase(space);

		if (size != 0) m_freeSpace.insert({ offset, size });

		//write next block offset
		if (fileSize != 0)
		{
			space = m_freeSpace.begin();
			m_fstream->write((char*)&space->offset, 8);
		}
		else
		{
			size_t temp = 0;
			m_fstream->write((char*)&temp, 8);
			break;
		}
	}

	FreeSpaceIter space = --m_freeSpace.end();
	m_curOffset = space->offset;
}

void EncryptedFolder::AddFolder(Folder<size_t>* parent, Folder<size_t>* newFolder, 
	bool(*callback)(File<size_t>*, Folder<size_t>*), std::string& key)
{
	Folder<size_t>* exist = parent->GetFolderByName(newFolder->Name());

	if (exist)
	{
		for (auto& f : newFolder->Folders())
		{
			AddFolder(exist, f, callback, key);
		}

		for (auto& f : newFolder->Files())
		{
			auto exf = exist->Add(f);
			if (exf)
			{
				if (callback(f, exist))
				{
					std::fstream fin(f->Path(), std::ios::binary | std::ios::in);

					AddFile(exf, fin, key);

					fin.close();
				}
				delete f;
			}
			else
			{
				std::fstream fin(f->Path(), std::ios::binary | std::ios::in);

				AddFile(f, fin, key);

				fin.close();
			}
		}

	}
	else
	{
		parent->AddNew(newFolder);

		newFolder->Traverse(
			[&](Folder<size_t>* current) 
			{
				for (auto& f : newFolder->Files())
				{
					std::fstream fin(f->Path(), std::ios::binary | std::ios::in);

					AddFile(f, fin, key);

					fin.close();
				}
			}, 
			nullptr);

	}

}

Folder<size_t>* EncryptedFolder::FindFolder(const std::wstring& destination)
{
	std::wstring path = MakeRelativePath(m_path, destination);

	if (path.substr(0, 3) == L"..\\")
	{
		path = path.substr(3);
	}

	if (path.substr(0, 2) == L".\\")
	{
		path = path.substr(2);
	}

	if (path.find(m_name) != std::wstring::npos)
	{
		path = L"";
	}

	return m_root->GetFolder(path);
}

File<size_t>* EncryptedFolder::FindFile(const std::wstring& destination)
{
	std::wstring path = MakeRelativePath(m_path, destination);

	if (path.substr(0, 3) == L"..\\")
	{
		path = path.substr(3);
	}

	if (path.substr(0, 2) == L".\\")
	{
		path = path.substr(2);
	}

	if (path.find(m_name) != std::wstring::npos)
	{
		path = L"";
	}

	return m_root->GetFile(path);
}

void EncryptedFolder::AddNewFile(const std::wstring& from, const std::wstring& to, std::string key)
{
	File<size_t>* newFile = new File<size_t>(from.c_str(), FILE_FLAG::FILE_PATH);

	auto where = FindFolder(to);

	if (where) where->AddNew(newFile);
	else Throw(L"\"" + to + L"\" not found.");

	std::fstream fin(from, std::ios::binary | std::ios::in);

	AddFile(newFile, fin, key);

	fin.close();

}

void EncryptedFolder::AddFileReplaceExists(const std::wstring& from, const std::wstring& to, std::string key)
{
	File<size_t>* newFile = new File<size_t>(from.c_str(), FILE_FLAG::FILE_PATH);

	auto where = FindFolder(to);

	File<size_t>* file = nullptr;

	if (where)
	{
		file = where->Add(newFile);
		if (file)
		{
			//newFile exists
			delete newFile;
		}
		else
		{
			file = newFile;
		}
	}
	else Throw(L"\"" + to + L"\" not found.");

	//mark file as deleted
	Remove(file);

	std::fstream fin(from, std::ios::binary | std::ios::in);

	AddFile(file, fin, key);

	fin.close();

}

void EncryptedFolder::AddFolder(const std::wstring& from, const std::wstring& to, 
	std::string key, bool(*callback)(File<size_t>*, Folder<size_t>*))
{
	auto where = FindFolder(to);

	if (!where) Throw(L"\"" + to + L"\" not found.");

	Folder<size_t>* newfolder = new Folder<size_t>(from.c_str());

	key.resize(32);
	AddFolder(where, newfolder, callback, key);

}

bool EncryptedFolder::RemoveFile(const std::wstring& path)
{
	size_t index = path.find_last_of(L'\\');

	auto des = path.substr(0, index);
	auto fileName = path.substr(index + 1);

	auto folder = FindFolder(des);

	if (folder)
	{
		auto file = folder->RemoveFile(fileName);
		if (file)
		{
			Remove(file);
			delete file;
			return true;
		}
	}

	return false;
}

bool EncryptedFolder::RemoveFolder(const std::wstring& path)
{
	auto folder = FindFolder(path);

	if (folder->Parent() == nullptr) Throw(L"Root folder can not be deleted.");

	if (folder)
	{
		//folder->MoveTo(folder->Parent()->Path());
		folder->Traverse(
			[&](Folder<size_t>* current)
			{
				for (auto& f : current->Files())
				{
					Remove(f);
					delete f;
				}
			}, 
			nullptr
		);

		folder->Parent()->RemoveFolder(folder->Name());

		delete folder;

		return true;
	}

	return false;
}

size_t EncryptedFolder::GetFileSize(File<size_t>* file)
{
	size_t curOffset = m_fstream->tellg();

	size_t offset = file->Data();

	size_t size = 0;
	size_t total = 0;

	while (offset)
	{
		m_fstream->seekg(offset + _EXTRA_DATA - 8, m_fstream->beg);
		m_fstream->read((char*)&size, 8);

		total += size;

		m_fstream->seekg(offset + _EXTRA_DATA + size, m_fstream->beg);
		m_fstream->read((char*)&offset, 8);
	}

	m_fstream->seekg(curOffset, m_fstream->beg);

	return total;
}

size_t EncryptedFolder::GetSize(File<size_t>* file)
{
	size_t curOffset = m_fstream->tellg();

	size_t offset = file->Data();

	size_t size = 0;
	size_t total = 0;

	while (offset)
	{
		m_fstream->seekg(offset + _EXTRA_DATA - 8, m_fstream->beg);
		m_fstream->read((char*)&size, 8);

		total += size + _EXTRA_DATA + 8;

		m_fstream->seekg(offset + _EXTRA_DATA + size, m_fstream->beg);
		m_fstream->read((char*)&offset, 8);
	}

	m_fstream->seekg(curOffset, m_fstream->beg);

	return total;
}

void EncryptedFolder::Read(File<size_t>* file, char* buffer)
{
	size_t curOffset = m_fstream->tellg();

	size_t offset = file->Data();

	size_t size = 0;

	auto cur = buffer;

	while (offset)
	{
		m_fstream->seekg(offset, m_fstream->beg);

		m_fstream->read(cur, _EXTRA_DATA);
		cur += _EXTRA_DATA;

		size = *(size_t*)(cur - 8);
		
		m_fstream->read(cur, size + 8);
		cur += size + 8;

		offset = *(size_t*)(cur - 8);

	}

	m_fstream->seekg(curOffset, m_fstream->beg);

}

void EncryptedFolder::Read(size_t& out_curPos, size_t& out_remainSize, char* buffer, size_t bufferSize)
{
	
}

void EncryptedFolder::DecryptFile(File<size_t>* file, std::fstream& fstream, std::string& key)
{
	//std::fstream fout(path, std::ios::binary | std::ios::out);

	m_fstream->seekg(file->Data(), m_fstream->beg);

	size_t nextOffset = Decrypt(*m_fstream, fstream, key, m_decryptor);

	while (nextOffset != 0)
	{
		m_fstream->seekg(nextOffset, m_fstream->beg);
		nextOffset = Decrypt(*m_fstream, fstream, key, m_decryptor);
	}
}

void EncryptedFolder::DecryptFolder(Folder<size_t>* folder, const std::wstring& to, std::string& key)
{
	key.resize(32);
	folder->Traverse(
		[&](Folder<size_t>* current)
		{
			CreateNewDirectory(CombinePath(to, current->Path()));
		},
		nullptr
	);

	folder->Traverse(
		[&](Folder<size_t>* current)
		{
			for (auto& f : current->Files())
			{
				auto outpath = CombinePath(to, f->Path());
				std::fstream fout(outpath, std::ios::binary | std::ios::out);

				m_fstream->seekg(f->Data(), m_fstream->beg);
				size_t nextOffset = Decrypt(*m_fstream, fout, key, m_decryptor);

				while (nextOffset != 0)
				{
					m_fstream->seekg(nextOffset, m_fstream->beg);
					nextOffset = Decrypt(*m_fstream, fout, key, m_decryptor);
				}

				fout.close();
			}
		},
		nullptr
	);
}

void EncryptedFolder::DecryptFile(const std::wstring& from, std::fstream& fstream, std::string& key)
{
	auto file = FindFile(from);

	if (!file) Throw(L"FindFile() return nullptr.");

	key.resize(32);
	DecryptFile(file, fstream, key);
}

void EncryptedFolder::DecryptFile(const std::wstring& from, const std::wstring& to, std::string key)
{
	auto file = FindFile(from);

	if (!file) Throw(L"FindFile() return nullptr.");

	key.resize(32);

	std::fstream fout(to, std::ios::binary | std::ios::out);
	DecryptFile(file, fout, key);
}

void EncryptedFolder::DecryptFolder(const std::wstring& from, const std::wstring& to, std::string key)
{
	auto folder = FindFolder(from);

	if (!folder) Throw(L"FindFolder() return nullptr.");

	key.resize(32);

	DecryptFolder(folder, to, key);

}

}
