// Pingus - A free Lemmings clone
// Copyright (C) 1999 Ingo Ruhnke <grumbel@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "util/system.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string.h>

#ifdef __APPLE__
#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/CFString.h>
#endif

#ifdef WINRT
#include <ppltasks.h>

#include <locale>
#include <codecvt>
#include <ppltasks.h>

#include <regex>

#include "util/winrt/task_synchronizer.hpp"

using namespace concurrency;
using namespace Windows::System;
using namespace Windows::Foundation::Collections;
#endif

#ifndef WIN32
#  include <dirent.h>
#  include <fcntl.h>
#  include <fnmatch.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#  include <errno.h>
#  include <boost/filesystem.hpp>
#else /* WIN32 */
#  include <windows.h>
#  include <direct.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <io.h>
#define chdir  _chdir
#define access _access
#define F_OK   0
#endif

#include "pingus/globals.hpp"
#include "util/log.hpp"
#include "util/pathname.hpp"
#include "util/raise_exception.hpp"
#include "util/string_util.hpp"

std::string System::userdir;
std::string System::default_email;
std::string System::default_username;

System::DirectoryEntry::DirectoryEntry(const std::string& n, FileType t)
	: type(t), name(n)
{
}

std::string
System::cut_file_extension(const std::string& filename)
{
	for (int i = static_cast<int>(filename.size()) - 1; i >= 0; --i)
	{
		if (filename[static_cast<size_t>(i)] == '.')
		{
			return filename.substr(0, static_cast<size_t>(i));
		}
		else if (filename[static_cast<size_t>(i)] == '/')
		{
			return filename;
		}
	}

	return filename;
}

std::string
System::get_file_extension(const std::string& filename)
{
	for (int i = static_cast<int>(filename.size()) - 1; i >= 0; --i)
	{
		if (filename[static_cast<size_t>(i)] == '.')
		{
			return filename.substr(static_cast<size_t>(i + 1));
		}
		else if (filename[static_cast<size_t>(i)] == '/')
		{
			return std::string();
		}
	}

	return std::string();
}

System::Directory
System::opendir(const std::string& pathname, const std::string& pattern)
{
	Directory dir_list;

#ifndef WIN32
	DIR* dp;
	dirent* de;

	dp = ::opendir(pathname.c_str());

	if (dp == 0)
	{
		raise_exception(std::runtime_error, pathname << ": " << strerror(errno));
	}
	else
	{
		while ((de = ::readdir(dp)) != 0)
		{
			if (fnmatch(pattern.c_str(), de->d_name, FNM_PATHNAME) == 0)
			{
				struct stat buf;
				stat((Pathname::join(pathname, de->d_name)).c_str(), &buf);

				if (strcmp(de->d_name, "..") != 0 &&
					strcmp(de->d_name, ".") != 0)
				{
					if (S_ISDIR(buf.st_mode))
					{
						dir_list.push_back(DirectoryEntry(de->d_name, DE_DIRECTORY));
					}
					else
					{
						dir_list.push_back(DirectoryEntry(de->d_name, DE_FILE));
					}
				}
			}
		}

		closedir(dp);
	}
#elif WINRT

	Windows::Storage::StorageFolder^ local_data_folder =
		Windows::Storage::ApplicationData::Current->LocalFolder;

	Windows::Storage::StorageFolder^ installed_data_folder =
		Windows::ApplicationModel::Package::Current->InstalledLocation;

	Platform::String^ folder_display_path = local_data_folder->Path;
	std::wstring temp_wstring(folder_display_path->Begin());

	std::string app_path(temp_wstring.begin(), temp_wstring.end());
	std::string directory_path = pathname;

	app_path.append("\\");

	std::replace(app_path.begin(), app_path.end(), '\\', '/');
	std::replace(directory_path.begin(), directory_path.end(), '\\', '/');

	std::regex regex_pattern(app_path);

	auto replaced_path = std::regex_replace(directory_path, regex_pattern, "");

	std::wstring intermediateForm(replaced_path.begin(), replaced_path.end());

	std::replace(intermediateForm.begin(), intermediateForm.end(), '/', '\\');

	Platform::String^ folder_path = ref new Platform::String(intermediateForm.c_str());
	

	Windows::Storage::StorageFolder^ local_folder = nullptr;
	Windows::Storage::StorageFolder^ data_folder = nullptr;

	std::thread([&local_folder, &local_data_folder, &folder_path]() {
		try {
			auto file_task = create_task(local_data_folder->GetFolderAsync(folder_path));
			file_task.wait();
			local_folder = file_task.get();
			
			//local_folder = PerformSynchronously(local_data_folder->GetFolderAsync(folder_path));
		}
		catch (Platform::COMException^ e) {
			local_folder = nullptr;
			
		}
	}).join();

	
	
	/*try {
		local_folder = PerformSynchronously(local_data_folder->GetFolderAsync(folder_path));
	}
	catch (Platform::Exception^ ex) {
		local_folder = nullptr;
	}*/


	std::thread([&data_folder, &installed_data_folder, &folder_path]() {
		try {

			auto file_task = create_task(installed_data_folder->GetFolderAsync(folder_path));
			file_task.wait();
			data_folder = file_task.get();

			//data_folder = PerformSynchronously(installed_data_folder->GetFolderAsync(folder_path));
		}
		catch (Platform::COMException^ ex) {
			data_folder = nullptr;
		}

	}).join();
		

	IVectorView<Windows::Storage::StorageFolder^>^ folders = nullptr;

	IVectorView<Windows::Storage::StorageFile^>^ files = nullptr;

	if (local_folder != nullptr) {
		folders = PerformSynchronously(local_folder->GetFoldersAsync());
		files = PerformSynchronously(local_folder->GetFilesAsync());
	} else if(data_folder != nullptr) {
		folders = PerformSynchronously(data_folder->GetFoldersAsync());
		files = PerformSynchronously(data_folder->GetFilesAsync());
	}

	if (folders != nullptr) {
		for (unsigned int i = 0; i < folders->Size; ++i) {

			Platform::String^ folder_file = folders->GetAt(i)->DisplayName;

			std::wstring temp_wstring(folder_file->Begin());
			std::string file_path(temp_wstring.begin(), temp_wstring.end());

			dir_list.push_back(DirectoryEntry(file_path, System::DE_DIRECTORY));
		}
	}

	if (files != nullptr) {

		for (unsigned int i = 0; i < files->Size; ++i) {

			Platform::String^ file = files->GetAt(i)->DisplayName;
			Platform::String^ extension = files->GetAt(i)->FileType;

			std::wstring temp_file(file->Begin());
			std::string file_path(temp_file.begin(), temp_file.end());

			std::wstring temp_extension(extension->Begin());
			std::string file_extension(temp_extension.begin(), temp_extension.end());

			file_path = file_path + file_extension;

			dir_list.push_back(DirectoryEntry(file_path, System::DE_FILE));
		}
	
	}
		




#elif WIN32 /* WIN32 */
	WIN32_FIND_DATA coFindData;
	std::string FindFileDir = Pathname::join(pathname, pattern);
	HANDLE hFind = FindFirstFile(TEXT(FindFileDir.c_str()), &coFindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			log_error("System: Couldn't open: %1%", pathname);
	}
	else
	{
		do
		{
			if (strcmp(coFindData.cFileName, "..") != 0 &&
				strcmp(coFindData.cFileName, ".") != 0)
			{
				if (coFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					dir_list.push_back(DirectoryEntry(coFindData.cFileName, System::DE_DIRECTORY));
				}
				else
				{
					dir_list.push_back(DirectoryEntry(coFindData.cFileName, System::DE_FILE));
				}
			}
		} while (FindNextFile(hFind, &coFindData));

		FindClose(hFind);
	}
#endif

	return dir_list;
}

std::vector<std::string>
System::opendir_recursive(const std::string& pathname)
{
	std::vector<std::string> lst;
	try
	{
		Directory dir = opendir(pathname);
		for (auto it = dir.begin(); it != dir.end(); ++it)
		{
			if (it->type == DE_DIRECTORY)
			{
				std::vector<std::string> subdir = opendir_recursive(Pathname::join(pathname, it->name));
				lst.insert(lst.end(), subdir.begin(), subdir.end());
			}
			else if (it->type == DE_FILE)
			{
				lst.push_back(Pathname::join(pathname, it->name));
			}
		}
	}
	catch (const std::exception& err)
	{
		log_warn("%1%", err.what());
	}
	return lst;
}

// Returns the basic filename without the path
std::string
System::basename(std::string filename)
{
	// Should be replaced with STL
	const char* str = filename.c_str();
	int i;

	for (i = static_cast<int>(filename.size()) - 1; i >= 0; --i)
	{
		if (*(str + i) == '/') {
			break;
		}
	}

	return (str + i + 1);
}

std::string
System::dirname(std::string filename)
{
	const char* str = filename.c_str();
	int i;

	for (i = static_cast<int>(filename.size()) - 1; i >= 0; --i)
	{
		if (*(str + i) == '/') {
			break;
		}
	}

	return filename.substr(0, static_cast<size_t>(i));
}

bool
System::exist(std::string filename)
{
	return !access(filename.c_str(), F_OK);
}

void
System::create_dir(std::string directory)
{
#ifndef WIN32
	if (!exist(directory))
	{
		std::string::iterator end = directory.end() - 1;
		if (*end == '/') {
			directory.erase(end);
		}
		log_info("System::create_dir: %1", directory);
		if (!boost::filesystem::create_directories(directory.c_str()))
		{
			raise_exception(std::runtime_error, "System::create_dir: " << directory << ": " << strerror(errno));
		}
		else
		{
			log_info("Successfully created: %1%", directory);
		}
	}
#elif WINRT
	Windows::Storage::StorageFolder^ appFolder =
		Windows::Storage::ApplicationData::Current->LocalFolder;

	Platform::String^ folder_display_path = appFolder->Path;
	std::wstring temp_wstring(folder_display_path->Begin());

	std::string app_path(temp_wstring.begin(), temp_wstring.end());
	std::string directory_path = directory;

	app_path.append("\\");

	std::replace(app_path.begin(), app_path.end(), '\\', '/');
	std::replace(directory_path.begin(), directory_path.end(), '\\', '/');

	std::regex pattern(app_path);

	auto replaced_path = std::regex_replace(directory_path, pattern, "");

	auto result = std::make_shared<std::vector<std::string> >();

	std::regex regex("/");

	std::sregex_token_iterator it(replaced_path.begin(), replaced_path.end(), regex, -1);
	std::sregex_token_iterator reg_end;

	for (; it != reg_end; ++it) {
		if (!it->str().empty()) //token could be empty:check
			result->emplace_back(it->str());
	}

	Windows::Storage::StorageFolder^ folder = appFolder;
	auto current_directory = std::make_shared<std::string>("");

	for (auto i = std::make_shared<unsigned int>(0); *i < result->size(); ++(*i)) {
		
		if (*current_directory != "") {
			std::wstring_convert<std::codecvt_utf8<wchar_t>> read_converter;
			std::wstring read_intermediate_form = read_converter.from_bytes(*current_directory);
			Platform::String^ read_directory = ref new Platform::String(read_intermediate_form.c_str());


			auto storage_folder = PerformSynchronously(folder->GetFolderAsync(read_directory));


			std::string sub_path = result->at(*i);

			std::wstring write_intermediate_form(sub_path.begin(), sub_path.end());
			Platform::String^ write_directory = ref new Platform::String(write_intermediate_form.c_str());

			current_directory->append("\\");
			current_directory->append(sub_path);

			auto written_folder = PerformSynchronously(storage_folder->CreateFolderAsync(write_directory,
				Windows::Storage::CreationCollisionOption::OpenIfExists));

		} else {
			std::string sub_path = result->at(*i);

			//std::wstring_convert<std::codecvt_utf8<wchar_t>> write_converter;
			//std::wstring write_intermediate_form = write_converter.from_bytes(sub_path);
			std::wstring write_intermediate_form(sub_path.begin(), sub_path.end());
			Platform::String^ write_directory = ref new Platform::String(write_intermediate_form.c_str());

			auto written_folder = PerformSynchronously(folder->CreateFolderAsync(write_directory,
				Windows::Storage::CreationCollisionOption::OpenIfExists));

			current_directory->append(sub_path);
		}

		/*std::string sub_path = result[i];

		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		std::wstring intermediateForm = converter.from_bytes(sub_path);
		Platform::String^ creation_directory = ref new Platform::String(intermediateForm.c_str());

		concurrency::
			create_task(folder->
						CreateFolderAsync(creation_directory, 
										  Windows::Storage::CreationCollisionOption::OpenIfExists))
			.then([&creation_folder](Windows::Storage::StorageFolder^ new_creation_folder) {
			
			creation_folder = ref new Windows::Storage::StorageFolder();

			creation_folder = new_creation_folder;
		}).wait();

		folder = creation_folder;*/
	}   


#elif WIN32
	if (!CreateDirectory(directory.c_str(), 0))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_ALREADY_EXISTS)
		{
		}
		else if (dwError == ERROR_PATH_NOT_FOUND)
		{
			raise_exception(std::runtime_error,
				"CreateDirectory: " << directory <<
				": One or more intermediate directories do not exist; this function will only create the final directory in the path.");
		}
		else
		{
			raise_exception(std::runtime_error,
				"CreateDirectory: " << directory << ": failed with error " << StringUtil::to_string(dwError));
		}
	}
	else
	{
		log_info("Successfully created: %1%", directory);
	}
#endif
}

std::string
System::find_userdir()
{
#ifdef WINRT

	std::string tmpstr;

	Windows::Storage::StorageFolder^ appFolder =
		Windows::Storage::ApplicationData::Current->LocalFolder;

	
	Platform::String^ folder_file = appFolder->Path;

	std::wstring temp_wstring(folder_file->Begin());
	std::string file_path(temp_wstring.begin(), temp_wstring.end());

	char* appdata = const_cast<char*>(file_path.c_str());

	if (appdata)
	{
		tmpstr = std::string(appdata) + "/Pingus-0.8/";
		for (size_t pos = tmpstr.find('\\', 0); pos != std::string::npos; pos = tmpstr.find('\\', 0))
			tmpstr[pos] = '/';
	}
	else
	{
		tmpstr = "user/";
	}

	return tmpstr;

#elif WIN32
	std::string tmpstr;
	char* appdata = getenv("APPDATA");
	if (appdata)
	{
		tmpstr = std::string(appdata) + "/Pingus-0.8/";
		for (size_t pos = tmpstr.find('\\', 0); pos != std::string::npos; pos = tmpstr.find('\\', 0))
			tmpstr[pos] = '/';
	}
	else
	{
		tmpstr = "user/";
	}

	return tmpstr;

#elif __APPLE__
	char* homedir = getenv("HOME");

	if (homedir)
	{
		return std::string(homedir) + "/Library/Application Support/pingus-0.8/";
	}
	else
	{
		raise_exception(std::runtime_error, "Environment variable $HOME not set, fix that and start again.");
	}

#else /* !WIN32 */
	// If ~/.pingus/ exist, use that for backward compatibility reasons,
	// if it does not, use $XDG_CONFIG_HOME, see:
	// http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html

#if 0
	// FIXME: insert code here to handle backward compatibilty with 0.7.x releases
	{ // check for ~/.pingus/
		char* homedir = getenv("HOME");
		if (homedir && strcmp(homedir, "") != 0)
		{
			std::string old_pingus_config_dir = std::string(homedir) + "/.pingus/";
			if (exist(old_pingus_config_dir))
			{
				return old_pingus_config_dir;
			}
		}
	}
#endif

	char* config_dir = getenv("XDG_CONFIG_HOME");
	if (!config_dir || strcmp(config_dir, "") == 0)
	{
		char* homedir = getenv("HOME");
		if (homedir && strcmp(homedir, "") != 0)
		{
			return std::string(homedir) + "/.config/pingus-0.8/";
		}
		else
		{
			raise_exception(std::runtime_error, "can't find userdir as neither $HOME nor $XDG_CONFIG_HOME is set");
		}
	}
	else
	{
		return std::string(config_dir) + "/pingus-0.8/";
	}
#endif
}

void
System::init_directories()
{
	if (userdir.empty())
		userdir = find_userdir();

	std::string statdir = get_userdir();

	// FIXME: We need a better seperation between user created levels,
	// FIXME: third party levels and levels from the base distri

	create_dir(statdir + "levels/");

#ifndef WINRT
	create_dir(statdir + "levels/dist");
#endif

	create_dir(statdir + "themes/");

	// Savegames (FIXME: rename to savegames/?)
	create_dir(statdir + "savegames/");

	// User created images
	create_dir(statdir + "images/");

	// Thumbnail cache
	create_dir(statdir + "cache/");

	// Recorded demos will per default be writen in this directory
	create_dir(statdir + "demos/");

	// User created images
	create_dir(statdir + "backup/");

	// Screenshots will be dumped to that directory:
	create_dir(statdir + "screenshots/");
}

void
System::set_userdir(const std::string& u)
{
	userdir = u + "/";
}

std::string
System::get_userdir()
{
	return userdir;
}

std::string
System::get_cachedir()
{
	return get_userdir() + "cache/";
}

/** Returns the username of the current user or an empty string */
std::string
System::get_username()
{
	if (default_username.empty())
	{
#ifdef WINRT
		char* username = nullptr;

		std::string str_username = "";

		if (UserProfile::UserInformation::NameAccessAllowed) {
			concurrency::create_task
			(UserProfile::UserInformation::GetDisplayNameAsync())
				.then([&str_username](Platform::String^ displayName) {

				std::wstring temp_wstring(displayName->Begin());
				std::string username_acquired(temp_wstring.begin(), temp_wstring.end());

				str_username = username_acquired;

			}).wait();
		}

		if (!str_username.empty()) {
			username = const_cast<char*>(str_username.c_str());
		}
#else
		char* username = getenv("USERNAME");
#endif

		if (username)
			return std::string(username);
		else
			return "";
	}
	else
	{
		return default_username;
	}
}

/** Returns the EMail of the user or an empty string */
std::string
System::get_email()
{
	if (default_email.empty())
	{

#ifdef WINRT
		char* email = nullptr;
#else
		char* email = getenv("EMAIL");
#endif

		if (email)
			// FIXME: $EMAIL contains the complete from header, not only
			// the email address
			return std::string(email);
		else
			return "";
	}
	else
	{
		return default_email;
	}
}

std::string
System::get_language()
{
#ifdef WINRT

	char* lang_c = nullptr;

	std::string str_lang = "";

	auto first_language = UserProfile::GlobalizationPreferences::Languages->GetAt(0);

	{
		std::wstring temp_wstring(first_language->Begin());
		std::string acquired_string(temp_wstring.begin(), temp_wstring.end());
		str_lang = acquired_string;
	}

	if (!str_lang.empty()) {
		lang_c = const_cast<char*>(str_lang.c_str());
	}

#elif WIN32
	char* lang_c = getenv("LC_MESSAGES");
#else
	char* lang_c = setlocale(LC_MESSAGES, NULL);
#endif
	std::string lang;

	if (lang_c)
	{
		lang = lang_c;
	}

	if (lang.empty() || lang == "C")
	{
#ifdef WINRT

		auto input_language = UserProfile::GlobalizationPreferences::Languages->GetAt(0);

		{
			std::wstring temp_wstring(input_language->Begin());
			std::string acquired_string(temp_wstring.begin(), temp_wstring.end());
			str_lang = acquired_string;
		}

		if (!str_lang.empty()) {
			lang_c = const_cast<char*>(str_lang.c_str());
		}
#else
		lang_c = getenv("LANG");
#endif
		if (lang_c)
		{
			lang = lang_c;
		}
	}

	if (lang.empty() || lang == "C")
	{
#ifndef __APPLE__
		return globals::default_language;
#else /* on Mac OS X we get "C" if launched using Finder, so we ask the OS for the language */
		/* Note: this is used as last resort to allow the use of LANG when starting via Terminal */
		CFArrayRef preferred_languages = CFLocaleCopyPreferredLanguages();
		//CFShow(preferred_languages);
		CFStringRef language = (CFStringRef)CFArrayGetValueAtIndex(preferred_languages, 0 /* most important language */);
		//CFShow(language);
		CFRelease(preferred_languages);
		CFStringRef substring = CFStringCreateWithSubstring(NULL, language, CFRangeMake(0, 2));
		CFRelease(language);
		char buff[3];
		CFStringGetCString(substring, buff, 3, kCFStringEncodingUTF8);
		CFRelease(substring);
		lang = buff;
		return lang;
#endif
	}
	else
	{
		return lang.substr(0, 2);
	}
}

std::string
System::checksum(const Pathname& pathname)
{
	return checksum(pathname.get_sys_path());
}

/** Read file and create a checksum and return it */
std::string
System::checksum(std::string filename)
{ // FIXME: Replace sys with SHA1 or MD5 or so
	FILE* in;
	size_t bytes_read;
	char buffer[4096];
	long int checksum = 0;

	in = fopen(filename.c_str(), "r");

	if (!in)
	{
		log_error("System::checksum: Couldn't open file: %1%", filename);
		return "";
	}

	do
	{
		bytes_read = fread(buffer, sizeof(char), 4096, in);

		if (bytes_read != 4096 && !feof(in))
		{
			raise_exception(std::runtime_error, "System:checksum: file read error");
		}

		for (size_t i = 0; i < bytes_read; ++i)
			checksum = checksum * 17 + buffer[i];
	} while (bytes_read != 0);

	fclose(in);

	std::ostringstream str;
	str << checksum;
	return str.str();
}

uint64_t
System::get_mtime(const std::string& filename)
{
#ifndef WIN32

	struct stat stat_buf;
	if (stat(filename.c_str(), &stat_buf) == 0)
		return static_cast<uint64_t>(stat_buf.st_mtime);
	else
		return 0;

#else

	struct _stat stat_buf;
	if (_stat(filename.c_str(), &stat_buf) == 0)
		return stat_buf.st_mtime;
	else
		return 0;

#endif
}

std::string
System::realpath(const std::string& pathname)
{
	std::string fullpath;

#ifdef WIN32
	std::string drive;
	if (pathname.size() > 2 && pathname[1] == ':' && pathname[2] == '/')
	{
		// absolute path on Win32
		drive = pathname.substr(0, 2);
		fullpath = pathname.substr(2);
	}
#else
	if (pathname.size() > 0 && pathname[0] == '/')
	{
		// absolute path on Linux
		fullpath = pathname;
	}
#endif
	else
	{
		// relative path
#ifdef WINRT
		Windows::Storage::StorageFolder^ appFolder =
			Windows::Storage::ApplicationData::Current->LocalFolder;


		Platform::String^ folder_file = appFolder->Path;

		std::wstring temp_wstring(folder_file->Begin());
		std::string file_path(temp_wstring.begin(), temp_wstring.end());

		char* cwd = const_cast<char*>(file_path.c_str());

#else
		char* cwd = getcwd(NULL, 0);
#endif
		if (!cwd)
		{
			log_error("System::realpath: Error: couldn't getcwd()");
			return pathname;
		}
		else
		{
#ifdef WIN32
			// unify directory separator to '/'
			for (char *p = cwd; *p; ++p)
			{
				if (*p == '\\')
					*p = '/';
			}
			drive.assign(cwd, 2);
			fullpath = Pathname::join(std::string(cwd + 2), pathname);
#else
			fullpath = Pathname::join(std::string(cwd), pathname);
#endif
			free(cwd);
		}
	}

#ifdef WIN32
	return drive + normalize_path(fullpath);
#else
	return normalize_path(fullpath);
#endif
}

namespace {

	void handle_directory(const std::string& dir, int& skip, std::string& result)
	{
		if (dir.empty() || dir == ".")
		{
			// ignore
		}
		else if (dir == "..")
		{
			skip += 1;
		}
		else
		{
			if (skip == 0)
			{
				if (result.empty())
				{
					result.append(dir);
				}
				else
				{
					result.append("/");
					result.append(dir);
				}
			}
			else
			{
				skip -= 1;
			}
		}
	}

} // namespaces

std::string
System::normalize_path(const std::string& path)
{
	if (path.empty())
	{
		return std::string();
	}
	else
	{
		bool absolute = false;
		if (path[0] == '/')
		{
			absolute = true;
		}

		std::string result;
		result.reserve(path.size());

		std::string::const_reverse_iterator last_slash = path.rbegin();
		int skip = 0;

		for (std::string::const_reverse_iterator i = path.rbegin(); i != path.rend(); ++i)
		{
			if (*i == '/')
			{
				std::string dir(last_slash, i);

				handle_directory(dir, skip, result);

				last_slash = i + 1;
			}
		}

		// relative path name
		if (last_slash != path.rend())
		{
			std::string dir(last_slash, path.rend());
			handle_directory(dir, skip, result);
		}

		if (!absolute)
		{
			for (int i = 0; i < skip; ++i)
			{
				if (i == skip - 1)
				{
					if (result.empty())
					{
						result.append("..");
					}
					else
					{
						result.append("/..");
					}
				}
				else
				{
					result.append("../");
				}
			}
		}
		else // if (absolute)
		{
			result += "/";
		}

		std::reverse(result.begin(), result.end());
		return result;
	}
}

void
System::write_file(const std::string& filename, const std::string& content)
{
	log_debug("writing %1%", filename);

#ifdef WIN32
	// FIXME: not save
	std::ofstream out(filename);
	out.write(content.data(), content.size());
#else
	// build the filename: "/home/foo/outfile.pngXXXXXX"
	std::unique_ptr<char[]> tmpfile(new char[filename.size() + 6 + 1]);
	strcpy(tmpfile.get(), filename.c_str());
	strcpy(tmpfile.get() + filename.size(), "XXXXXX");

	// create a temporary file
	mode_t old_mask = umask(S_IRWXO | S_IRWXG);
	int fd = mkstemp(tmpfile.get());
	umask(old_mask);
	if (fd < 0)
	{
		raise_exception(std::runtime_error, tmpfile.get() << ": " << strerror(errno));
	}

	// write the data to the temporary file
	if (write(fd, content.data(), content.size()) < 0)
	{
		raise_exception(std::runtime_error, tmpfile.get() << ": " << strerror(errno));
	}

	if (close(fd) < 0)
	{
		raise_exception(std::runtime_error, tmpfile.get() << ": " << strerror(errno));
	}

	// rename the temporary file to it's final location
	if (rename(tmpfile.get(), filename.c_str()) < 0)
	{
		raise_exception(std::runtime_error, tmpfile.get() << ": " << strerror(errno));
	}

	// adjust permissions to normal default permissions, as mkstemp
	// might not honor umask
	if (chmod(filename.c_str(), ~old_mask & 0666) < 0)
	{
		raise_exception(std::runtime_error, tmpfile.get() << ": " << strerror(errno));
	}
#endif
}

/* EOF */