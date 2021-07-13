#include "FileSystemProtectionAPI.h"

#include <sstream>

#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/sha.h>

#include "FileFolder.h"

#include "../Common/File.h"

unsigned char* AESEncryptBlock(EVP_CIPHER_CTX* e, unsigned char* plaintext, int* len)
{
	int c_len = *len + AES_BLOCK_SIZE, f_len = 0;
	unsigned char* ciphertext = (unsigned char*)malloc(c_len);

	EVP_EncryptUpdate(e, ciphertext, &c_len, plaintext, *len);

	EVP_EncryptFinal_ex(e, ciphertext + c_len, &f_len);

	*len = c_len + f_len;
	return ciphertext;
}

//return num bytes written to buffer
inline int AESEncryptBlock(EVP_CIPHER_CTX* e, unsigned char* plaintext, int len, unsigned char* buffer)
{
	int c_len = len + AES_BLOCK_SIZE, f_len = 0;

	EVP_EncryptUpdate(e, buffer, &c_len, plaintext, len);

	EVP_EncryptFinal_ex(e, buffer + c_len, &f_len);

	return  c_len + f_len;
}

std::string BytesToHexString(const std::vector<uint8_t>& bytes)
{
	std::ostringstream stream;
	for (uint8_t b : bytes)
	{
		stream << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(b);
	}
	return stream.str();
}

std::string SHA3_512(uint8_t* buffer, int len)
{
	uint32_t length = SHA512_DIGEST_LENGTH;
	const EVP_MD* algorithm = EVP_sha3_512();
	uint8_t* digest = static_cast<uint8_t*>(OPENSSL_malloc(length));
	EVP_MD_CTX* context = EVP_MD_CTX_new();
	EVP_DigestInit_ex(context, algorithm, nullptr);
	EVP_DigestUpdate(context, buffer, len);
	EVP_DigestFinal_ex(context, digest, &length);
	EVP_MD_CTX_destroy(context);

	std::string output = BytesToHexString(std::vector<uint8_t>(digest, digest + length));
	OPENSSL_free(digest);
	return output;
}

#ifdef EncryptFile
#undef EncryptFile
#endif // EncryptFile

#ifdef DecryptFile
#undef DecryptFile
#endif

//if file < thres encypt all file in 1 cycle
//else encrypt 1 blockSize block per cycle
void EncryptFile(const std::wstring& in_path, const std::wstring& out_path, 
	std::string key, std::string IV, EVP_CIPHER_CTX* ctx,
	size_t thres, size_t blockSize)
{
	std::ifstream fin(in_path, std::ios::binary | std::ios::in);
	std::ofstream fout(out_path, std::ios::out | std::ios::binary);

	if (IV.size() != 32) IV.resize(32);
	if (key.size() != 32) key.resize(32);

	bool flag = false;
	if (ctx == nullptr)
	{
		ctx = EVP_CIPHER_CTX_new();
		EVP_CIPHER_CTX_init(ctx);
		EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());
		//EVP_CIPHER_CTX_set_padding(ctx, EVP_PADDING_PKCS7);
		flag = true;
	}
	
	fout.write(IV.c_str(), 32);

	size_t fileSize = GetFileSize(in_path);
	if (fileSize < thres)
	{
		char* buffer = new char[fileSize];

		char* cipherBuffer = new char[fileSize + AES_BLOCK_SIZE];

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

		long long numChar = blockSize;
		numChar = fin.read(buffer, blockSize).gcount();

		int outSize;

		while (numChar == blockSize)
		{
			EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, blockSize);

			fout.write(cipherBuffer, outSize);

			numChar = fin.read(buffer, blockSize).gcount();
		}

		EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, numChar);
		fout.write(cipherBuffer, outSize);

		int foutSize;

		EVP_EncryptFinal_ex(ctx, (uint8_t*)cipherBuffer, &foutSize);

		fout.write(cipherBuffer, foutSize);

		delete[] cipherBuffer;
		delete[] buffer;

	}

	fin.close();
	fout.close();

	if (flag)
	{
		EVP_CIPHER_CTX_free(ctx);
	}

}

size_t EncryptFile(std::ifstream& fin, std::ofstream& fout, std::string key, std::string IV,
	EVP_CIPHER_CTX* ctx, size_t thres, size_t blockSize)
{
	if (IV.size() != 32) IV.resize(32);
	if (key.size() != 32) key.resize(32);

	bool flag = false;
	if (ctx == nullptr)
	{
		ctx = EVP_CIPHER_CTX_new();
		EVP_CIPHER_CTX_init(ctx);
		EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());
		//EVP_CIPHER_CTX_set_padding(ctx, EVP_PADDING_PKCS7);
		flag = true;
	}
	else
	{
		EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());
	}

	fout.write(IV.c_str(), 32);

	fin.seekg(0, fin.end);
	size_t fileSize = fin.tellg();
	fin.seekg(0, fin.beg);

	//cipherLen = (clearLen/16 + 1) * 16; //for CBC, CTR mode
	//write len of cipher text
	size_t cipherSize = fileSize;
	//fout.write((char*)cipherSize, 8);

	if (fileSize < thres)
	{
		char* buffer = new char[fileSize];

		char* cipherBuffer = new char[fileSize + AES_BLOCK_SIZE];

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

		long long numChar = blockSize;
		numChar = fin.read(buffer, blockSize).gcount();

		int outSize;

		while (numChar == blockSize)
		{
			EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, blockSize);

			fout.write(cipherBuffer, outSize);

			numChar = fin.read(buffer, blockSize).gcount();
		}

		EVP_EncryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, numChar);
		fout.write(cipherBuffer, outSize);

		int foutSize;

		EVP_EncryptFinal_ex(ctx, (uint8_t*)cipherBuffer, &foutSize);

		fout.write(cipherBuffer, foutSize);

		delete[] cipherBuffer;
		delete[] buffer;

	}

	if (flag)
	{
		EVP_CIPHER_CTX_free(ctx);
	}

	return cipherSize;
}

size_t DecryptFile(std::ifstream& fin, std::ofstream& fout, std::string key, 
	EVP_CIPHER_CTX* ctx, size_t thres, size_t blockSize)
{
	if (key.size() != 32) key.resize(32);

	char IVBuff[33] = {};
	fin.read(IVBuff, 32);

	bool flag = false;
	if (ctx == nullptr)
	{
		ctx = EVP_CIPHER_CTX_new();
		EVP_CIPHER_CTX_init(ctx);
		EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);
		//EVP_CIPHER_CTX_set_padding(ctx, EVP_PADDING_PKCS7);
		flag = true;
	}
	else
	{
		EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);
	}

	fin.seekg(0, fin.end);
	size_t fileSize = fin.tellg();
	fileSize -= 32;
	fin.seekg(32, fin.beg);

	size_t numbytesWritten = 0;

	if (fileSize < thres)
	{
		char* buffer = new char[fileSize];

		char* cipherBuffer = new char[fileSize + AES_BLOCK_SIZE];

		int outSize;

		fin.read(buffer, fileSize);

		EVP_DecryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, fileSize);

		int foutSize;

		EVP_DecryptFinal_ex(ctx, (uint8_t*)(cipherBuffer + outSize), &foutSize);

		numbytesWritten = foutSize + outSize;

		fout.write((const char*)cipherBuffer, numbytesWritten);	

		delete[] cipherBuffer;
		delete[] buffer;
	}
	else
	{
		char* buffer = new char[blockSize];
		char* cipherBuffer = new char[blockSize + AES_BLOCK_SIZE];

		long long numChar = blockSize;
		numChar = fin.read(buffer, blockSize).gcount();

		int outSize;

		while (numChar == blockSize)
		{
			EVP_DecryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, blockSize);

			fout.write(cipherBuffer, outSize);
			numbytesWritten += outSize;

			numChar = fin.read(buffer, blockSize).gcount();
		}

		EVP_DecryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, numChar);
		fout.write(cipherBuffer, outSize);
		numbytesWritten += outSize;

		int foutSize;

		EVP_DecryptFinal_ex(ctx, (uint8_t*)cipherBuffer, &foutSize);

		fout.write(cipherBuffer, foutSize);
		numbytesWritten += foutSize;

		delete[] cipherBuffer;
		delete[] buffer;

	}

	if (flag)
	{
		EVP_CIPHER_CTX_free(ctx);
	}

	return numbytesWritten;
}

size_t DecryptFile(std::ifstream& fin, std::ofstream& fout, size_t fileSize, 
	std::string key, EVP_CIPHER_CTX* ctx, size_t thres, size_t blockSize)
{
	if (key.size() != 32) key.resize(32);

	char IVBuff[33] = {};
	fin.read(IVBuff, 32);

	bool flag = false;
	if (ctx == nullptr)
	{
		ctx = EVP_CIPHER_CTX_new();
		EVP_CIPHER_CTX_init(ctx);
		EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);
		//EVP_CIPHER_CTX_set_padding(ctx, EVP_PADDING_PKCS7);
		flag = true;
	}
	else
	{
		EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);
	}

	size_t numbytesWritten = 0;

	if (fileSize < thres)
	{
		char* buffer = new char[fileSize];

		char* cipherBuffer = new char[fileSize + AES_BLOCK_SIZE];

		int outSize;

		fin.read(buffer, fileSize);

		EVP_DecryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, fileSize);

		int foutSize;

		EVP_DecryptFinal_ex(ctx, (uint8_t*)(cipherBuffer + outSize), &foutSize);

		numbytesWritten = foutSize + outSize;

		fout.write((const char*)cipherBuffer, numbytesWritten);

		delete[] cipherBuffer;
		delete[] buffer;
	}
	else
	{
		char* buffer = new char[blockSize];
		char* cipherBuffer = new char[blockSize + AES_BLOCK_SIZE];

		long long numChar = blockSize;
		numChar = fin.read(buffer, blockSize).gcount();

		int outSize;

		long long total = fileSize;
		total -= blockSize;

		while (total >= blockSize)
		{
			EVP_DecryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, blockSize);

			fout.write(cipherBuffer, outSize);
			numbytesWritten += outSize;

			numChar = fin.read(buffer, blockSize).gcount();
			total -= blockSize;
		}

		EVP_DecryptUpdate(ctx, (uint8_t*)cipherBuffer, &outSize, (uint8_t*)buffer, total);
		fout.write(cipherBuffer, outSize);
		numbytesWritten += outSize;

		int foutSize;

		EVP_DecryptFinal_ex(ctx, (uint8_t*)cipherBuffer, &foutSize);

		fout.write(cipherBuffer, foutSize);
		numbytesWritten += foutSize;

		delete[] cipherBuffer;
		delete[] buffer;

	}

	if (flag)
	{
		EVP_CIPHER_CTX_free(ctx);
	}

	return numbytesWritten;
}


void EncryptFolder(Folder<std::string>* folder, const std::wstring& out_path,
	std::string key, std::string IV)
{
	EVP_CIPHER_CTX* e_ctx = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_init(e_ctx);

	if (IV.empty()) IV = RandomString(32);

	IV.resize(32);
	key.resize(32);
	
	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());

	uint8_t bufferName[MAX_PATH] = {};
	uint8_t bufferTime[sizeof(long long) / sizeof(char) + 1] = {};

	CreateNewDirectory(out_path);
	
	folder->Traverse(
		[&](Folder<std::string>* current) 
		{
			for (auto& f : current->Files())
			{
				//auto name = f->Name();

				long long time = std::chrono::high_resolution_clock::now().time_since_epoch().count();

				*(long long*)bufferTime = time;

				std::string tempIv = RandomString(32);
				EVP_EncryptInit_ex(e_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)tempIv.c_str());
				auto length = AESEncryptBlock(e_ctx, (uint8_t*)bufferTime, sizeof(long long) / sizeof(char) + 1, bufferName);
				bufferName[length] = '\0';

				f->Data() = BytesToHexString(std::vector<uint8_t>(bufferName, bufferName + length));
				EncryptFile(f->Path(), out_path + L"\\" + StringToWString(f->Data()), key, RandomString(32));
			}
		}, 
		nullptr);

	folder->ToRelativePath();

	std::wstring masterRecord = folder->ToStructureString(
		[](std::wostream& fout, File<std::string>* file)
		{
			fout << StringToWString(file->Data());
			//fout << StringToWString("File data.");
		});

	int recordLen = masterRecord.length() * 2;

	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());
	uint8_t* ciphertext = AESEncryptBlock(e_ctx, (uint8_t*)masterRecord.c_str(), &recordLen);

	std::ofstream fout(out_path + L"\\MasterRecord.est", std::ios::binary | std::ios::out);

	fout.write(IV.c_str(), 32);

	fout.write((const char*)ciphertext, recordLen);

	fout.close();

	delete ciphertext;

	EVP_CIPHER_CTX_free(e_ctx);
}

Folder<std::string>* OpenEncryptedFolder(const std::wstring& encrpytedFolderPath, std::string key)
{
	auto path = encrpytedFolderPath + L"\\MasterRecord.est";
	std::ifstream fin(path, std::ios::binary | std::ios::in);

	char IVBuff[33] = {};

	fin.read(IVBuff, 32);

	EVP_CIPHER_CTX* d_ctx = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_init(d_ctx);

	key.resize(32);

	EVP_DecryptInit_ex(d_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);

	auto fileSize = GetFileSize(path) - 32;

	char* fileContent = new char[fileSize];

	fin.read(fileContent, fileSize);

	char* plainText = new char[fileSize + AES_BLOCK_SIZE]();
	int plainTextSize;

	EVP_DecryptUpdate(d_ctx, (uint8_t*)plainText, &plainTextSize, (uint8_t*)fileContent, fileSize);

	int fSize;

	EVP_DecryptFinal_ex(d_ctx, (uint8_t*)(plainText + plainTextSize), &fSize);

	std::wstring content = (const wchar_t*)plainText;

	delete[] fileContent;
	delete[] plainText;

	std::wstringstream ss(content);

	Folder<std::string>* root = new Folder<std::string>();

	root->LoadStructure(ss, 
		[](std::wistream& win, File<std::string>* file) 
		{
			std::wstring line;
			std::getline(win, line);
			file->Data() = WStringToString(line);
		});

	EVP_CIPHER_CTX_free(d_ctx);

	fin.close();

	return root;
}

std::wstring DecryptFolder(const std::wstring& in_path, const std::wstring& out_path,
	std::string key)
{
	auto folder = OpenEncryptedFolder(in_path, key);

	key.resize(32);

	folder->Traverse(
		[&](Folder<std::string>* current)
		{
			CreateNewDirectory(CombinePath(out_path, current->Path()));
		},
		nullptr
	);

	folder->Traverse(
		[&](Folder<std::string>* current)
		{
			for (auto& f : current->Files())
			{
				auto outpath = in_path + L'\\' + StringToWString(f->Data());
				std::ifstream fin(outpath, std::ios::binary | std::ios::in);

				auto inpath = CombinePath(out_path, f->Path());
				std::ofstream fout(inpath, std::ios::binary | std::ios::out);

				auto fileSize = DecryptFile(fin, fout, key);

				fin.close();
				fout.close();
			}
		},
		nullptr
	);

	std::wstring name = folder->Name();

	delete folder;

	return name;

}

void EncryptFolderToFile(Folder<_ExtraData>* folder, const std::wstring& out_path, std::string key, std::string IV)
{
	std::ofstream fout(out_path, std::ios::binary | std::ios::out);

	EVP_CIPHER_CTX* e_ctx = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_init(e_ctx);

	if (IV.empty()) IV = RandomString(32);

	IV.resize(32);
	key.resize(32);

	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());

	uint8_t bufferName[MAX_PATH] = {};
	uint8_t bufferTime[sizeof(long long) / sizeof(char) + 1] = {};

	//offset to master record
	fout.write("1234567812345678", 16);

	size_t count = 16;

	folder->Traverse(
		[&](Folder<_ExtraData>* current)
		{
			for (auto& f : current->Files())
			{
				f->Data().offset = count;

				std::ifstream fin(f->Path(), std::ios::binary | std::ios::in);
				f->Data().size = EncryptFile(fin, fout, key, IV);

				//32 bytes for IV
				count += f->Data().size + 32;

				fin.close();
			}
		},
		nullptr);

	folder->ToRelativePath();


	std::wstring masterRecord = folder->ToStructureString(
		[](std::wostream& out, File<_ExtraData>* file)
		{
			auto& data = file->Data();
			out.write((wchar_t*)&data.offset, sizeof(data.offset) / sizeof(wchar_t));
			out.write((wchar_t*)&data.size, sizeof(data.size) / sizeof(wchar_t));
		});

	int recordLen = masterRecord.length() * 2;

	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IV.c_str());
	uint8_t* ciphertext = AESEncryptBlock(e_ctx, (uint8_t*)masterRecord.c_str(), &recordLen);

	fout.write(IV.c_str(), 32);

	fout.write((const char*)ciphertext, recordLen);

	fout.seekp(0, fout.beg);
	fout.write((char*)&count, 8);
	fout.write((char*)&recordLen, 8);

	fout.close();

	delete ciphertext;

	EVP_CIPHER_CTX_free(e_ctx);
}

Folder<_ExtraData>* OpenEncryptedFolderFromFile(const std::wstring& encrpytedFolderPath, std::string key)
{
	std::ifstream fin(encrpytedFolderPath, std::ios::binary | std::ios::in);

	char longBuffer[9] = {};

	fin.read(longBuffer, 8);
	auto offset = *(size_t*)longBuffer;

	memset(longBuffer, 0, 9);
	fin.read(longBuffer, 8);
	auto materRecordSize = *(int*)longBuffer - 32;

	fin.seekg(offset, fin.beg);

	char IVBuff[33] = {};

	fin.read(IVBuff, 32);

	EVP_CIPHER_CTX* d_ctx = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_init(d_ctx);

	key.resize(32);

	EVP_DecryptInit_ex(d_ctx, EVP_aes_256_gcm(), NULL, (uint8_t*)key.c_str(), (uint8_t*)IVBuff);

	char* materRecordContent = new char[materRecordSize];

	fin.read(materRecordContent, materRecordSize);

	char* plainText = new char[materRecordSize + AES_BLOCK_SIZE]();
	int plainTextSize;

	EVP_DecryptUpdate(d_ctx, (uint8_t*)plainText, &plainTextSize, (uint8_t*)materRecordContent, materRecordSize);

	int fSize;

	EVP_DecryptFinal_ex(d_ctx, (uint8_t*)(plainText + plainTextSize), &fSize);

	std::wstring content = std::wstring((const wchar_t*)plainText, (const wchar_t*)(plainText + plainTextSize));

	delete[] materRecordContent;
	delete[] plainText;

	std::wstringstream ss(content);

	Folder<_ExtraData>* root = new Folder<_ExtraData>();

	root->LoadStructure(ss,
		[](std::wistream& win, File<_ExtraData>* file)
		{
			auto& data = file->Data();
			win.read((wchar_t*)&data.offset, sizeof(data.offset) / sizeof(wchar_t));
			win.read((wchar_t*)&data.size, sizeof(data.size) / sizeof(wchar_t));
			std::wstring line;
			std::getline(win, line);

		});

	EVP_CIPHER_CTX_free(d_ctx);

	fin.close();

	return root;
}

std::wstring DecryptFolderFromFile(const std::wstring& in_path, const std::wstring& out_path, std::string key)
{
	auto folder = OpenEncryptedFolderFromFile(in_path, key);

	key.resize(32);

	std::ifstream fin(in_path, std::ios::binary | std::ios::in);

	folder->Traverse(
		[&](Folder<_ExtraData>* current)
		{
			CreateNewDirectory(CombinePath(out_path, current->Path()));
		},
		nullptr
	);

	folder->Traverse(
		[&](Folder<_ExtraData>* current)
		{
			for (auto& f : current->Files())
			{
				auto outpath = CombinePath(out_path, f->Path());
				std::ofstream fout(outpath, std::ios::binary | std::ios::out);

				fin.seekg(f->Data().offset, fin.beg);

				auto fileSize = DecryptFile(fin, fout, f->Data().size, key);

				fout.close();
			}
		},
		nullptr
	);

	fin.close();

	std::wstring name = folder->Name();

	delete folder;

	return name;

}