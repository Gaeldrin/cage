#include <cage-core/string.h>
#include <cage-core/timer.h>
#include <cage-core/concurrent.h> // threadSleep
#include <cage-core/flatSet.h>

#include "files.h"

#ifdef CAGE_SYSTEM_WINDOWS
#include "../incWin.h"
#include <io.h> // _get_osfhandle
#define fseek _fseeki64
#define ftell _ftelli64
#include <vector> // wide characters
#else
#define _FILE_OFFSET_BITS 64
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#endif
#ifdef CAGE_SYSTEM_MAC
#include <mach-o/dyld.h>
#endif

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <FileWatcher/FileWatcher.h>

namespace cage
{
	namespace
	{
#ifdef CAGE_SYSTEM_WINDOWS
		struct Widen
		{
			Widen(const String &path)
			{
				auto len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), path.size(), nullptr, 0);
				if (len <= 0)
					CAGE_THROW_ERROR(cage::SystemError, "MultiByteToWideChar", GetLastError());
				data.resize(len + 1);
				auto ret = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), path.size(), data.data(), data.size());
				CAGE_ASSERT(ret == len);
				data[ret] = 0;
			}

			operator const wchar_t *() const
			{
				return data.data();
			}

			std::vector<wchar_t> data;
		};

		String narrow(PointerRange<const wchar_t> path)
		{
			String data;
			auto res = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), path.size(), data.rawData(), String::MaxLength - 1, nullptr, nullptr);
			if (res == 0)
				CAGE_THROW_ERROR(cage::SystemError, "WideCharToMultiByte", GetLastError());
			data.rawData()[res] = 0;
			data.rawLength() = res;
			return data;
		}
#endif
	}

	Holder<DirectoryList> realNewDirectoryList(const String &path);

	PathTypeFlags realType(const String &path)
	{
#ifdef CAGE_SYSTEM_WINDOWS

		auto a = GetFileAttributesW(Widen(path));
		if (a == INVALID_FILE_ATTRIBUTES)
			return PathTypeFlags::NotFound;
		if ((a & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			return PathTypeFlags::Directory;
		else
			return PathTypeFlags::File;

#else

		struct stat st;
		if (stat(path.c_str(), &st) != 0)
			return PathTypeFlags::NotFound;
		if ((st.st_mode & S_IFDIR) == S_IFDIR)
			return PathTypeFlags::Directory;
		if ((st.st_mode & S_IFREG) == S_IFREG)
			return PathTypeFlags::File;
		return PathTypeFlags::None;

#endif
	}

	void realCreateDirectories(const String &path)
	{
		String pth = path + "/";
		uint32 off = 0;
		while (true)
		{
			uint32 pos = find(subString(pth, off, m), '/');
			if (pos == m)
				return; // done
			pos += off;
			off = pos + 1;
			if (pos)
			{
				const String p = subString(pth, 0, pos);
				if (any(realType(p) & PathTypeFlags::Directory))
					continue;

#ifdef CAGE_SYSTEM_WINDOWS
				if (CreateDirectoryW(Widen(p), nullptr) == 0)
				{
					const auto err = GetLastError();
					if (err != ERROR_ALREADY_EXISTS)
					{
						CAGE_LOG_THROW(Stringizer() + "path: '" + path + "'");
						CAGE_THROW_ERROR(SystemError, "CreateDirectory", err);
					}
				}
#else
				static constexpr mode_t mode = 0755;
				if (mkdir(p.c_str(), mode) != 0 && errno != EEXIST)
				{
					CAGE_LOG_THROW(Stringizer() + "path: '" + path + "'");
					CAGE_THROW_ERROR(Exception, "mkdir");
				}
#endif
			}
		}
	}

	void realMove(const String &from, const String &to)
	{
		pathCreateDirectories(pathExtractDirectory(to));

#ifdef CAGE_SYSTEM_WINDOWS

		auto res = MoveFileW(Widen(from), Widen(to));
		if (res == 0)
		{
			CAGE_LOG_THROW(Stringizer() + "path from: '" + from + "'" + ", to: '" + to + "'");
			CAGE_THROW_ERROR(SystemError, "pathMove", GetLastError());
		}

#else

		auto res = rename(from.c_str(), to.c_str());
		if (res != 0)
		{
			CAGE_LOG_THROW(Stringizer() + "path from: '" + from + "'" + ", to: '" + to + "'");
			CAGE_THROW_ERROR(SystemError, "pathMove", errno);
		}

#endif
	}

	void realRemove(const String &path)
	{
		const PathTypeFlags t = realType(path);
		if (any(t & PathTypeFlags::Directory))
		{
			{
				Holder<DirectoryList> list = realNewDirectoryList(path);
				while (list->valid())
				{
					realRemove(pathJoin(path, list->name()));
					list->next();
				}
			}

#ifdef CAGE_SYSTEM_WINDOWS
			if (RemoveDirectoryW(Widen(path)) == 0)
				CAGE_THROW_ERROR(SystemError, "RemoveDirectory", GetLastError());
#else
			if (rmdir(path.c_str()) != 0)
				CAGE_THROW_ERROR(SystemError, "rmdir", errno);
#endif
		}
		else if (none(t & PathTypeFlags::NotFound))
		{
#ifdef CAGE_SYSTEM_WINDOWS
			if (DeleteFileW(Widen(path)) == 0)
				CAGE_THROW_ERROR(SystemError, "DeleteFile", GetLastError());
#else
			if (unlink(path.c_str()) != 0)
				CAGE_THROW_ERROR(SystemError, "unlink", errno);
#endif
		}
	}

	uint64 realLastChange(const String &path)
	{
#ifdef CAGE_SYSTEM_WINDOWS

		HANDLE hFile = CreateFileW(Widen(path), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			CAGE_LOG_THROW(Stringizer() + "path: '" + path + "'");
			CAGE_THROW_ERROR(Exception, "failed to retrieve file last modification time");
		}
		FILETIME ftWrite;
		if (!GetFileTime(hFile, nullptr, nullptr, &ftWrite))
		{
			CloseHandle(hFile);
			CAGE_LOG_THROW(Stringizer() + "path: '" + path + "'");
			CAGE_THROW_ERROR(Exception, "failed to retrieve file last modification time");
		}
		ULARGE_INTEGER l;
		l.LowPart = ftWrite.dwLowDateTime;
		l.HighPart = ftWrite.dwHighDateTime;
		CloseHandle(hFile);
		return l.QuadPart;

#else

		struct stat st;
		if (stat(pathToAbs(path).c_str(), &st) == 0)
			return st.st_mtime;
		CAGE_LOG_THROW(Stringizer() + "path: '" + path + "'");
		CAGE_THROW_ERROR(SystemError, "stat", errno);

#endif
	}

	namespace
	{
		String pathWorkingDirImpl()
		{
#ifdef CAGE_SYSTEM_WINDOWS

			wchar_t buffer[String::MaxLength + 1];
			uint32 len = GetCurrentDirectoryW(String::MaxLength, buffer);
			if (len <= 0)
				CAGE_THROW_ERROR(SystemError, "GetCurrentDirectory", GetLastError());
			return pathSimplify(narrow({ buffer, buffer + len }));

#else

			char buffer[String::MaxLength + 1];
			if (getcwd(buffer, String::MaxLength) != nullptr)
				return pathSimplify(buffer);
			CAGE_THROW_ERROR(Exception, "getcwd");

#endif
		}
	}

	String pathWorkingDir()
	{
		static const String dir = pathWorkingDirImpl();
		CAGE_ASSERT(dir == pathWorkingDirImpl());
		return dir;
	}

	namespace
	{
		String executableFullPathImpl()
		{
#ifdef CAGE_SYSTEM_WINDOWS

			wchar_t buffer[String::MaxLength];
			uint32 len = GetModuleFileNameW(nullptr, (wchar_t *)&buffer, String::MaxLength);
			if (len == 0)
				CAGE_THROW_ERROR(SystemError, "GetModuleFileName", GetLastError());
			return pathSimplify(narrow({ buffer, buffer + len }));

#elif defined(CAGE_SYSTEM_LINUX)

			char buffer[String::MaxLength];
			char id[String::MaxLength];
			sprintf(id, "/proc/%d/exe", getpid());
			sint32 len = readlink(id, buffer, String::MaxLength);
			if (len == -1)
				CAGE_THROW_ERROR(SystemError, "readlink", errno);
			return pathSimplify(String({ buffer, buffer + len }));

#elif defined(CAGE_SYSTEM_MAC)

			char buffer[String::MaxLength];
			uint32 len = sizeof(buffer);
			if (_NSGetExecutablePath(buffer, &len) != 0)
				CAGE_THROW_ERROR(Exception, "_NSGetExecutablePath");
			len = detail::strlen(buffer);
			return pathSimplify(String({ buffer, buffer + len }));

#else

#error This operating system is not supported

#endif
		}
	}

	namespace detail
	{
		String executableFullPath()
		{
			static const String pth = executableFullPathImpl();
			return pth;
		}

		String executableFullPathNoExe()
		{
#ifdef CAGE_SYSTEM_WINDOWS
			String p = executableFullPath();
			CAGE_ASSERT(isPattern(toLower(p), "", "", ".exe"));
			return subString(p, 0, p.length() - 4);
#else
			return executableFullPath();
#endif
		}
	}

	namespace
	{
		class FileReal : public FileAbstract
		{
		public:
			FILE *f = nullptr;

			FileReal(const String &path, const FileMode &mode) : FileAbstract(path, mode)
			{
				CAGE_ASSERT(mode.valid());
				if (mode.write)
					realCreateDirectories(pathJoin(path, ".."));
#ifdef CAGE_SYSTEM_WINDOWS
				f = _wfopen(Widen(path), Widen(mode.mode()));
#else
				f = fopen(path.c_str(), mode.mode().c_str());
#endif
				if (!f)
				{
					CAGE_LOG_THROW(Stringizer() + "read: " + mode.read + ", write: " + mode.write + ", append: " + mode.append + ", text: " + mode.textual);
					CAGE_LOG_THROW(Stringizer() + "path: " + path);
					CAGE_THROW_ERROR(SystemError, "fopen", errno);
				}
			}

			~FileReal()
			{
				if (f)
				{
					try
					{
						close();
					}
					catch (const cage::Exception &)
					{
						// do nothing
					}
				}
			}

			void reopenForModification() override
			{
				CAGE_ASSERT(myMode.read && !myMode.write);
				CAGE_ASSERT(f);
				myMode.write = true;
#ifdef CAGE_SYSTEM_WINDOWS
				f = _wfreopen(Widen(myPath), Widen(myMode.mode()), f);
#else
				f = freopen(myPath.c_str(), myMode.mode().c_str(), f);
#endif
				if (!f)
				{
					CAGE_LOG_THROW(Stringizer() + "path: " + myPath);
					CAGE_THROW_ERROR(SystemError, "freopen", errno);
				}
			}

			void readAt(PointerRange<char> buffer, uintPtr at) override
			{
				CAGE_ASSERT(f);
				CAGE_ASSERT(myMode.read);
				if (buffer.size() == 0)
					return;

#ifdef CAGE_SYSTEM_WINDOWS
				OVERLAPPED o;
				detail::memset(&o, 0, sizeof(o));
				o.Offset = (DWORD)at;
				o.OffsetHigh = (DWORD)((uint64)at >> 32);
				DWORD r = 0;
				if (!ReadFile((HANDLE)_get_osfhandle(_fileno(f)), buffer.data(), numeric_cast<DWORD>(buffer.size()), &r, &o) || r != buffer.size())
					CAGE_THROW_ERROR(SystemError, "ReadFile", GetLastError());
#else
				if (pread(fileno(f), buffer.data(), buffer.size(), at) != buffer.size())
					CAGE_THROW_ERROR(SystemError, "pread", errno);
#endif
			}

			void read(PointerRange<char> buffer) override
			{
				CAGE_ASSERT(f);
				CAGE_ASSERT(myMode.read);
				if (buffer.size() == 0)
					return;
				if (fread(buffer.data(), buffer.size(), 1, f) != 1)
					CAGE_THROW_ERROR(SystemError, "fread", errno);
			}

			void write(PointerRange<const char> buffer) override
			{
				CAGE_ASSERT(f);
				CAGE_ASSERT(myMode.write);
				if (buffer.size() == 0)
					return;
				if (fwrite(buffer.data(), buffer.size(), 1, f) != 1)
					CAGE_THROW_ERROR(SystemError, "fwrite", errno);
			}

			void seek(uintPtr position) override
			{
				CAGE_ASSERT(f);
				if (fseek(f, position, 0) != 0)
					CAGE_THROW_ERROR(SystemError, "fseek", errno);
			}

			void close() override
			{
				CAGE_ASSERT(f);
				FILE *t = f;
				f = nullptr;
				if (fclose(t) != 0)
					CAGE_THROW_ERROR(SystemError, "fclose", errno);
			}

			uintPtr tell() override
			{
				CAGE_ASSERT(f);
				return numeric_cast<uintPtr>(ftell(f));
			}

			uintPtr size() override
			{
				CAGE_ASSERT(f);
				auto pos = ftell(f);
				fseek(f, 0, 2);
				auto siz = ftell(f);
				fseek(f, pos, 0);
				return numeric_cast<uintPtr>(siz);
			}
		};
	}

	Holder<File> realNewFile(const String &path, const FileMode &mode)
	{
		return systemMemory().createImpl<File, FileReal>(path, mode);
	}

	void realTryFlushFile(File *f_)
	{
		FileReal *f = dynamic_cast<FileReal *>((FileAbstract *)f_);
		if (f)
			fflush(f->f);
	}

	namespace
	{
		class DirectoryListReal : public DirectoryListAbstract
		{
		public:
			bool valid_ = false;

#ifdef CAGE_SYSTEM_WINDOWS
			WIN32_FIND_DATAW ffd;
			HANDLE list = nullptr;
#else
			DIR *pdir = nullptr;
			struct dirent *pent = nullptr;
#endif

			DirectoryListReal(const String &path) : DirectoryListAbstract(path)
			{
				realCreateDirectories(path);

#ifdef CAGE_SYSTEM_WINDOWS
				CAGE_ASSERT(!myPath.empty());
				list = FindFirstFileW(Widen(myPath + "/*"), &ffd);
				valid_ = list != INVALID_HANDLE_VALUE;
				if (!valid_)
					return;
				if (name() == "." || name() == "..")
					next();
#else
				pdir = opendir(path.c_str());
				valid_ = !!pdir;
				next();
#endif
			}

			~DirectoryListReal()
			{
#ifdef CAGE_SYSTEM_WINDOWS
				if (list)
					FindClose(list);
#else
				if (pdir)
					closedir(pdir);
#endif
			}

			bool valid() const override
			{
				return valid_;
			}

			String name() const override
			{
#ifdef CAGE_SYSTEM_WINDOWS
				return narrow({ ffd.cFileName, ffd.cFileName + wcslen(ffd.cFileName) });
#else
				return pent->d_name;
#endif
			}

			void next() override
			{
				CAGE_ASSERT(valid_);

#ifdef CAGE_SYSTEM_WINDOWS

				if (FindNextFileW(list, &ffd) == 0)
				{
					valid_ = false;
					return;
				}

#else

				pent = readdir(pdir);
				if (!pent)
				{
					valid_ = false;
					return;
				}

#endif

				if (name() == "." || name() == "..")
					next();
			}
		};
	}

	Holder<DirectoryList> realNewDirectoryList(const String &path)
	{
		return systemMemory().createImpl<DirectoryList, DirectoryListReal>(path);
	}

	namespace
	{
		class ArchiveReal : public ArchiveAbstract
		{
		public:
			ArchiveReal(const String &path) : ArchiveAbstract(path)
			{}

			PathTypeFlags type(const String &path) const
			{
				return realType(pathJoin(myPath, path));
			}

			void createDirectories(const String &path)
			{
				return realCreateDirectories(pathJoin(myPath, path));
			}

			void move(const String &from, const String &to)
			{
				realMove(pathJoin(myPath, from), pathJoin(myPath, to));
			}

			void remove(const String &path)
			{
				realRemove(pathJoin(myPath, path));
			}

			uint64 lastChange(const String &path) const
			{
				return realLastChange(pathJoin(myPath, path));
			}

			Holder<File> openFile(const String &path, const FileMode &mode)
			{
				return realNewFile(pathJoin(myPath, path), mode);
			}

			Holder<DirectoryList> listDirectory(const String &path) const
			{
				return realNewDirectoryList(pathJoin(myPath, path));
			}
		};
	}

	std::shared_ptr<ArchiveAbstract> archiveOpenReal(const String &path)
	{
		auto a = std::make_shared<ArchiveReal>(path);
		return a;
	}

	namespace
	{
		class FilesystemWatcherImpl : public FilesystemWatcher, private FW::FileWatchListener
		{
		public:
			FlatSet<String, StringComparatorFast> files;
			Holder<FW::FileWatcher> fw;
			Holder<Timer> clock;

			FilesystemWatcherImpl()
			{
				fw = systemMemory().createHolder<FW::FileWatcher>();
				clock = newTimer();
			}

			String waitForChange(uint64 time)
			{
				clock->reset();
				while (files.empty())
				{
					fw->update();
					if (files.empty() && clock->duration() > time)
						return "";
					else
						threadSleep(1000 * 100);
				}
				const String res = *files.begin();
				files.erase(files.begin());
				return res;
			}

			void handleFileAction(FW::WatchID watchid, const FW::String &dir, const FW::String &filename, FW::Action action) override
			{
				files.insert(pathJoin(dir.c_str(), filename.c_str()));
			}

			void registerPath(const String &path)
			{
				fw->addWatch(path.c_str(), this);
				Holder<DirectoryList> dl = newDirectoryList(path);
				while (dl->valid())
				{
					const String p = dl->fullPath();
					const PathTypeFlags type = realType(p);
					if (any(type & PathTypeFlags::Directory))
						registerPath(p);
					dl->next();
				}
			}
		};
	}

	void FilesystemWatcher::registerPath(const String &path_)
	{
		FilesystemWatcherImpl *impl = (FilesystemWatcherImpl *)this;
		const String path = pathToAbs(path_);
		const PathTypeFlags type = realType(path); // FilesystemWatcher works with real filesystem only!
		if (none(type & PathTypeFlags::Directory))
		{
			CAGE_LOG_THROW(Stringizer() + "path: '" + path + "'");
			CAGE_THROW_ERROR(Exception, "path must be existing folder");
		}
		impl->registerPath(path);
	}

	String FilesystemWatcher::waitForChange(uint64 time)
	{
		FilesystemWatcherImpl *impl = (FilesystemWatcherImpl *)this;
		return impl->waitForChange(time);
	}

	Holder<FilesystemWatcher> newFilesystemWatcher()
	{
		return systemMemory().createImpl<FilesystemWatcher, FilesystemWatcherImpl>();
	}
}
