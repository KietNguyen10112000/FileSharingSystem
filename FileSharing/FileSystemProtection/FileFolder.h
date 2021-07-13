#pragma once
#include <string>
#include <vector>
#include <iostream>

#include <filesystem>

namespace fs = std::filesystem;

#ifdef _DEBUG
#define Throw(msg) throw msg L"\nThrow from File \"" __FILEW__ L"\", Line " _STRINGIZE(__LINE__) L"."
#else
#define Throw(msg) throw msg
#endif // DEBUG

#include "../Common/File.h"


enum FOLDER_FLAG
{
	NO_SCAN = 1, 
	SCAN = 2,		//scan all file and folder from real storage
	LOAD_STRUCTURE = 4,  //load from structure file
	USE_RELATIVE_PATH = 8,
	FOLDER_NAME = 16,
};

enum FILE_FLAG
{
	FILE_NAME = 1,
	FILE_PATH = 2,
};

template<typename T> class File;

template<typename T>
class Folder
{
private:
	friend class File<T>;

public:
	Folder<T>* m_parent = nullptr;
	int m_index = -1; //index in parent sub folders

	std::wstring m_name;

	//std::wstring m_path;
	std::vector<Folder<T>*> m_subFolders;
	std::vector<File<T>*> m_files;

	std::wstring* m_iteratorPath = nullptr;

public:
	inline Folder() { m_iteratorPath = new std::wstring(); };
	Folder(const wchar_t* path, long long flag = FOLDER_FLAG::SCAN | FOLDER_FLAG::FOLDER_NAME, long long reserve = 0);
	~Folder();

public:
	//2 funcs must have same procotype
	//see PrettyPrint() for more detail
	template<typename _Func1, typename _Func2, typename ... _Args>
	void Traverse(_Func1 preFnc, _Func2 postFnc, _Args&& ... args);

public:
	template<typename T>
	friend std::wostream& operator<<(std::wostream& wcout, Folder<T>* folder);

	inline void PrettyPrint();

	//free all sub folders and files
	inline void RecursiveFree();
	//free this folder only
	inline void Free();

private:
	inline Folder<T>* Parse(std::wistream& win, void (*load)(std::wistream&, File<T>*) = nullptr);

public:
	inline void SaveStructure(std::wostream& wout, void (*)(std::wostream&, File<T>*) = nullptr);
	inline void LoadStructure(std::wistream& win, void (*)(std::wistream&, File<T>*) = nullptr);

	inline void SaveStructure(const wchar_t* savePath);
	inline void LoadStructure(const wchar_t* structureFilePath);

	//make all folder hover so when you call to MoveTo() it will hook to an another path
	inline void ToRelativePath();

	inline void MoveTo(const std::wstring& path);

	//like SaveStructure but not save to file
	inline std::wstring ToStructureString(void (*save)(std::wostream&, File<T>*) = nullptr);

public:
	inline Folder<T>* GetFolderByName(const std::wstring& name);
	inline Folder<T>* GetFolder(const std::wstring& path);

	inline File<T>* GetFileByName(const std::wstring& name);
	inline File<T>* GetFile(const std::wstring& path);

public:
	//throw if file exists
	inline void AddNew(File<T>* file);
	//replace if file exists
	//return null if file not exists, return file if it exists
	inline File<T>* Add(File<T>* file);

	//add folder only, not add its file, throw if folder exists
	inline void AddNew(Folder<T>* folder);
	//call callback() if file exists.
	template<typename _Func, typename ... _Args>
	inline void Add(Folder<T>* folder, _Func func, _Args&& ... args);

	//return the removed file, null if no file removed
	inline File<T>* RemoveFile(const std::wstring& fileName);

	inline Folder<T>* RemoveFolder(const std::wstring& folderName);

public:
	inline std::wstring Path();
	inline auto Name() { return m_name; };

	inline auto& Files() { return m_files; };
	inline auto& Folders() { return m_subFolders; };

	inline bool Empty() { return m_subFolders.empty() && m_files.empty(); };
	inline size_t FileCount() { return m_files.size(); };
	inline size_t FolderCount() { return m_subFolders.size(); };

	//index in parent sub folders
	inline auto Index() { return m_index; };
	inline auto Parent() { return m_parent; };

};

template<typename T>
class File
{
public:
	Folder<T>* m_parent = nullptr;
	std::wstring m_name;
	T m_data; //point to data of file

public:
	inline File() {};
	File(const wchar_t* param, FILE_FLAG flag = FILE_NAME, Folder<T>* parent = nullptr);
	~File();

public:
	template<typename T>
	friend std::wostream& operator<<(std::wostream& wcout, File<T>* file);

public:
	inline auto& Name() { return m_name; };
	inline auto& Data() { return m_data; };
	inline auto& ParentFolder() { return m_parent; };

	inline std::wstring Path() { return m_parent->Path() + L"\\" + m_name; };

};

template<typename T>
inline Folder<T>::Folder(const wchar_t* path, long long flag, long long reserve)
{
	if ((flag & (FOLDER_FLAG::FOLDER_NAME)) != 0)
	{
		m_name = GetFileName(path);
	}
	if ((flag & (FOLDER_FLAG::SCAN)) != 0)
	{
		if (!fs::is_directory(path))
		{
			Throw(L"Path must be Folder.");
		}
		try
		{
			for (const auto& entry : fs::directory_iterator(path))
			{
				if (entry.is_directory())
				{
					Folder<T>* temp = new Folder<T>(entry.path().c_str(), flag, reserve + 1);
					temp->m_index = m_subFolders.size();
					temp->m_parent = this;
					m_subFolders.push_back(temp);
				}
				else
				{
					File<T>* temp = new File<T>(entry.path().filename().c_str(), FILE_NAME, this);
					m_files.push_back(temp);
				}
			}
		}
		catch (fs::filesystem_error& e)
		{
			std::cerr << e.what() << "\n";
		}
	}

	if ((flag & (FOLDER_FLAG::LOAD_STRUCTURE)) != 0)
	{
		if (!fs::is_regular_file(path))
		{
			Throw(L"Path must be file.");
		}

		LoadStructure(path);
	}

	if (reserve == 0)
	{
		m_iteratorPath = new std::wstring(path);
		if (m_iteratorPath->back() == L'\\') m_iteratorPath->pop_back();
		//auto lpath = std::wstring(path);
		//MoveTo(lpath.substr(0, lpath.find_last_of(L"\\/")));
	}

}

template<typename T>
inline Folder<T>::~Folder()
{
	if (m_iteratorPath) delete m_iteratorPath;
}

template<typename T>
template<typename _Func1, typename _Func2, typename ... _Args>
inline void Folder<T>::Traverse(_Func1 preFunc, _Func2 postFunc, _Args&& ...args)
{
	//constexpr std::size_t n = sizeof...(_Args);
	size_t size = 0;
	if (m_iteratorPath)
	{
		size = m_iteratorPath->size();
		//*m_iteratorPath += L'\\' + m_name;
	}
	
	if constexpr (!(std::is_same_v<std::decay_t<_Func1>, std::nullptr_t>)) preFunc(this, std::forward<_Args>(args) ...);
	for (auto f : m_subFolders)
	{
		f->m_iteratorPath = m_iteratorPath;

		if (m_iteratorPath)
		{
			*m_iteratorPath += L'\\' + f->m_name;
		}

		f->Traverse(preFunc, postFunc, std::forward<_Args>(args) ...);

		f->m_iteratorPath = nullptr;

		if (m_iteratorPath)
		{
			m_iteratorPath->resize(size);
		}
	}
	if constexpr (!(std::is_same_v<std::decay_t<_Func2>, std::nullptr_t>)) postFunc(this, std::forward<_Args>(args) ...);
	/*if (m_iteratorPath)
	{
		m_iteratorPath->resize(size);
	}*/
}

template<typename T>
inline void Folder<T>::PrettyPrint()
{
	std::vector<std::pair<Folder<T>*, int>> stack;

	Traverse(
		[&](Folder<T>* current)
		{
			if (stack.empty())
			{
				std::wcout << current->Name() << " /\n";
				if (current->FileCount() == 0) stack.push_back({ current, 0 });
				else stack.push_back({ current, 1 });
			}
			else
			{
				std::wcout << ' ';
				for (size_t i = 0; i < stack.size() - 1; i++)
				{
					if (stack[i].second != 0) std::wcout << L"│    ";
					else std::wcout << "     ";
				}

				//bool last = false;
				if (current->Parent() != nullptr
					&& current->Parent()->FileCount() == 0
					&& current->Index() == current->Parent()->FolderCount() - 1)
				{
					std::wcout << L"└── " << current->Name() << " /\n";
					stack.back().second = 0;
					//last = true;
				}
				else
					std::wcout << L"├── " << current->Name() << " /\n";

				if (current->FileCount() == 0)
				{
					if(current->FolderCount() == 1)
						stack.push_back({ current, 0 });
					else
					{
						stack.push_back({ current, 1 });
					}
				}
				else stack.push_back({ current, 1 });
			}
		},
		[&](Folder<T>* current)
		{
			
			if (current->FileCount() != 0)
			{
				long long j = 0;
				for (j = 0; j < current->FileCount() - 1; j++)
				{
					std::wcout << ' ';
					for (size_t i = 0; i < stack.size() - 1; i++)
					{
						if (stack[i].second != 0) std::wcout << L"│    ";
						else std::wcout << "     ";
					}
					std::wcout << L"├── " << current->Files()[j]->Name() << "\n";
					//└──
				}
				std::wcout << ' ';
				for (size_t i = 0; i < stack.size() - 1; i++)
				{
					if (stack[i].second != 0) std::wcout << L"│    ";
					else std::wcout << "     ";
				}
				std::wcout << L"└── " << current->Files()[j]->Name() << "\n";
			}

			if (!stack.empty()) stack.pop_back();

		});
}

template<typename T>
inline void Folder<T>::RecursiveFree()
{
	Traverse(nullptr,
		[](Folder<T>* current)
		{
			for (auto& f : current->Folders())
			{
				delete f;
			}
			current->Folders().clear();
			for (auto& f : current->Files())
			{
				delete f;
			}
			current->Files().clear();
		});
}

template<typename T>
inline void Folder<T>::Free()
{
	for (auto& f : m_subFolders)
	{
		delete f;
	}
	m_subFolders.clear();
	for (auto& f : m_files)
	{
		delete f;
	}
	m_files.clear();
}

template<typename T>
inline void Folder<T>::SaveStructure(std::wostream& wout, void (*save)(std::wostream&, File<T>*))
{
	auto lo = wout.imbue(std::locale("en_US.utf8"));

	//write out the signature
	wout << L"FILESYSTEM_STRUCTURE\n";
	//wout << Name() << L'\n';

	Traverse(
		[&](Folder<T>* current)
		{
			wout << L"<Folder>\n";
			wout << current->Name() << L'\n';

			wout << current->FileCount() << L'\n';

			for (auto& f : current->Files())
			{
				wout << f->Name() << L'\n';
				if (save)
				{
					save(wout, f); wout << L'\n';
				}
			}

			wout << L"</Folder>\n";
		},
		[&](Folder<T>* current)
		{
			wout << L"<Pop />\n";
		});

}

template<typename T>
inline void Folder<T>::SaveStructure(const wchar_t* savePath)
{
	std::wofstream fout(savePath, std::ios::binary | std::ios::out);
	SaveStructure(fout);
	fout.close();
}

template<typename T>
inline Folder<T>* Folder<T>::Parse(std::wistream& win, void (*load)(std::wistream&, File<T>*))
{
	std::wstring path;
	std::getline(win, path);

	std::wstring line;
	std::getline(win, line);
	size_t fileCount = std::stoi(line);

	Folder<T>* temp = new Folder<T>();
	temp->m_name = path;

	for (size_t i = 0; i < fileCount; i++)
	{
		std::getline(win, line);
		File<T>* file = new File<T>();
		file->m_name = line;
		file->m_parent = temp;

		if (load) load(win, file);

		temp->m_files.push_back(file);
	}

	return temp;
}

template<typename T>
inline void Folder<T>::LoadStructure(std::wistream& win, void (*load)(std::wistream&, File<T>*))
{
	if (m_subFolders.size() != 0) RecursiveFree();

	std::vector<Folder<T>*> stack;

	auto lo = win.imbue(std::locale("en_US.utf8"));

	std::wstring line;

	std::getline(win, line);

	if (line != L"FILESYSTEM_STRUCTURE")
	{
		Throw("File must be a structure file.");
	}
	std::wstring oriName;
	std::getline(win, oriName);
	bool hasOriName = false;
	if (oriName != L"<Folder>")
	{
		std::getline(win, line);
		hasOriName = true;
	}
	Folder<T>* root = Parse(win, load);

	m_name = root->m_name;
	m_files.swap(root->m_files);

	for (auto& f : m_files)
	{
		f->m_parent = this;
	}

	delete root;

	stack.push_back(this);

	while (!win.eof())
	{
		std::getline(win, line);
		if (line == L"<Folder>")
		{
			Folder* parent = stack.back();
			Folder<T>* folder = Parse(win, load);

			folder->m_index = parent->m_subFolders.size();
			folder->m_parent = parent;
			parent->m_subFolders.push_back(folder);

			stack.push_back(folder);
		}
		else if (line == L"<Pop />")
		{
			stack.pop_back();
		}
	}

	if (hasOriName)
	{
		MoveTo(L"./" + oriName);
	}

}

template<typename T>
inline void Folder<T>::LoadStructure(const wchar_t* structureFilePath)
{
	std::wifstream fin(structureFilePath, std::ios::binary | std::ios::in);
	LoadStructure(fin);
	fin.close();
}

template<typename T>
inline void Folder<T>::ToRelativePath()
{
	/*std::wstring root = Path().substr(0, m_path.find_last_of(L"\\/"));
	Traverse(
		[&](Folder<T>* current)
		{
			current->m_path = MakeRelativePath(root, current->Path());
		},
		nullptr
	);*/
	if (m_iteratorPath)
	{
		*m_iteratorPath = L"";
	}
}

template<typename T>
inline void Folder<T>::MoveTo(const std::wstring& path)
{
	/*Traverse(
		[&](Folder<T>* current)
		{
			current->m_path = CombinePath(path, current->Path());
		},
		nullptr
	);*/
	if (!m_iteratorPath)
	{
		m_iteratorPath = new std::wstring();
	}
	*m_iteratorPath = path + L'\\' + m_name;
}

template<typename T>
inline std::wstring Folder<T>::ToStructureString(void (*save)(std::wostream&, File<T>*))
{
	std::wostringstream fout;
	SaveStructure(fout, save);
	return fout.str();
}

template<typename T>
inline Folder<T>* Folder<T>::GetFolderByName(const std::wstring& name)
{
	for (auto& f : m_subFolders)
	{
		if (f->Name() == name) return f;
	}
	return nullptr;
}

//path must be relative path
template<typename T>
inline Folder<T>* Folder<T>::GetFolder(const std::wstring& path)
{
	if (path.empty()) return this;

	fs::path relativePath(path);
	if (relativePath.is_absolute()) return nullptr;

	std::vector<std::wstring> way;

	StringSplit(path, L"\\", way);

	if (!way.empty() && (*way.begin()).find(L".") != std::wstring::npos) way.erase(way.begin());
	if (!way.empty() && *way.begin() == m_name) way.erase(way.begin());

	Folder<T>* cur = this;

	for (auto& name : way)
	{
		cur = cur->GetFolderByName(name);

		if (!cur)
		{
			return nullptr;
		}
	}

	return cur;
}

template<typename T>
inline File<T>* Folder<T>::GetFileByName(const std::wstring& name)
{
	for (auto& f : m_files)
	{
		if (f->Name() == name) return f;
	}
	return nullptr;
}

template<typename T>
inline File<T>* Folder<T>::GetFile(const std::wstring& path)
{
	if (path.empty()) return nullptr;

	fs::path relativePath(path);
	if (relativePath.is_absolute()) return nullptr;

	std::vector<std::wstring> way;

	StringSplit(path, L"\\", way);

	std::wstring fileName;

	if (way.empty()) return nullptr;
	else
	{
		fileName = way.back();
		way.pop_back();
	}

	if (!way.empty() && (*way.begin()).find(L".") != std::wstring::npos) way.erase(way.begin());
	if (!way.empty() && *way.begin() == m_name) way.erase(way.begin());

	Folder<T>* cur = this;

	for (auto& name : way)
	{
		cur = cur->GetFolderByName(name);

		if (!cur)
		{
			return nullptr;
		}
	}

	return cur->GetFileByName(fileName);
}

template<typename T>
inline void Folder<T>::AddNew(File<T>* file)
{
	for (auto& f : m_files)
	{
		if (f->Name() == file->Name())
		{
			Throw(L"File exists.");
		}
	}
	m_files.push_back(file);
	file->m_parent = this;
}

template<typename T>
inline File<T>* Folder<T>::Add(File<T>* file)
{
	for (auto& f : m_files)
	{
		if (f->Name() == file->Name())
		{
			return f;
		}
	}
	m_files.push_back(file);
	file->m_parent = this;
	return nullptr;
}

template<typename T>
inline void Folder<T>::AddNew(Folder<T>* folder)
{
	folder->m_index = m_subFolders.size();
	folder->m_parent = this;
	m_subFolders.push_back(folder);
}

template<typename T>
template<typename _Func, typename ..._Args>
inline void Folder<T>::Add(Folder<T>* folder, _Func func, _Args&& ...args)
{
	Folder<T>* newFolder = folder;
	Folder<T>* exists = nullptr;

	newFolder->ToRelativePath();

	for (auto& f : m_subFolders)
	{
		if (f->Name() == folder->Name())
		{
			exists = f;
			break;
		}
	}

	if (exists)
	{
		newFolder->Traverse(
			[&](Folder<T>* current)
			{
				for (auto& f : current->Files())
				{
					File<T>* file = exists->GetFile(f->Path());

					if (file)
					{
						if constexpr (!(std::is_same_v<std::decay_t<_Func>, std::nullptr_t>))
						{
							callback(file, current, std::forward<_Args>(args) ...);
						}
					}

				}

			},
			nullptr);
	}
	else
	{
		m_subFolders.push_back(newFolder);
		newFolder->m_parent = this;
	}
}

template<typename T>
inline File<T>* Folder<T>::RemoveFile(const std::wstring& fileName)
{
	File<T>* re = nullptr;

	typename std::vector<File<T>*>::iterator it;
	for (it = m_files.begin(); it != m_files.end(); it++)
	{
		if ((*it)->Name() == fileName)
		{
			re = (*it);
			break;
		}
	}

	if (re != nullptr)
	{
		m_files.erase(it);
	}

	return re;
}

template<typename T>
inline Folder<T>* Folder<T>::RemoveFolder(const std::wstring& folderName)
{
	Folder<T>* re = nullptr;

	typename std::vector<Folder<T>*>::iterator it;
	for (it = m_subFolders.begin(); it != m_subFolders.end(); it++)
	{
		if ((*it)->Name() == folderName)
		{
			re = (*it);
			break;
		}
	}

	if (re != nullptr)
	{
		m_subFolders.erase(it);
	}

	return re;
}

template<typename T>
inline std::wstring Folder<T>::Path()
{
	if (m_iteratorPath) return *m_iteratorPath;

	if (m_parent != nullptr) return m_parent->Path() + L'\\' + m_name; 
	else
	{
		return m_name;
	}
}

template<typename T>
inline File<T>::File(const wchar_t* param, FILE_FLAG flag, Folder<T>* parent)
{
	m_parent = parent;
	if ((flag & (FILE_FLAG::FILE_NAME)) != 0)
	{
		m_name = param;
	}

	if ((flag & (FILE_FLAG::FILE_PATH)) != 0)
	{
		/*if (parent == nullptr)
		{
			Throw(L"Parent folder can't be nullptr.");
		}*/
		if (m_parent != nullptr) m_parent->AddNew(this);

		m_name = param;
		auto index = m_name.find_last_of(L"\\/");
		
		m_name = m_name.substr(index + 1);

	}

	

}

template<typename T>
inline File<T>::~File()
{
}

template<typename T>
inline std::wostream& operator<<(std::wostream& wcout, Folder<T>* folder)
{
	folder->PrettyPrint();
	return wcout;
}

template<typename T>
inline std::wostream& operator<<(std::wostream& wcout, File<T>* file)
{
	return wcout << file->Path();
}

