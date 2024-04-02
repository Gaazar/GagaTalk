#include "windows.h"
#include "native_util.hpp"
#include <regex>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <WinHttp.h>
#undef min

namespace fs = std::filesystem;

#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Crypt32.lib")

namespace sutil
{
	std::string w2s(const std::wstring& wstr, unsigned int code_page)
	{
		std::string result;
		//获取缓冲区大小，并申请空间，缓冲区大小事按字节计算的  
		int len = WideCharToMultiByte(code_page, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
		char* buffer = new char[len + 1];
		//宽字节编码转换成多字节编码  
		WideCharToMultiByte(code_page, 0, wstr.c_str(), wstr.size(), buffer, len, NULL, NULL);
		buffer[len] = '\0';
		//删除缓冲区并返回值  
		result.append(buffer);
		delete[] buffer;
		return result;
	}

	std::wstring s2w(const std::string& str, unsigned int code_page)
	{
		std::wstring result;
		//获取缓冲区大小，并申请空间，缓冲区大小按字符计算  
		int len = MultiByteToWideChar(code_page, 0, str.c_str(), str.size(), NULL, 0);
		TCHAR* buffer = new TCHAR[len + 1];
		//多字节编码转换成宽字节编码  
		MultiByteToWideChar(code_page, 0, str.c_str(), str.size(), buffer, len);
		buffer[len] = '\0';             //添加字符串结尾  
		//删除缓冲区并返回值  
		result.append(buffer);
		delete[] buffer;
		return result;
	}
	std::wstring s2w(const char* str, size_t len)
	{
		std::wstring result;
		TCHAR* buffer = new TCHAR[len + 1];
		MultiByteToWideChar(CP_ACP, 0, str, len, buffer, len);
		result.append(buffer);
		delete[] buffer;
		return result;
	}
	std::string a2u8(const std::string& str)
	{
		return w2s(s2w(str, CP_ACP), CP_UTF8);
	}
	std::string u82a(const std::string& str)
	{
		return w2s(s2w(str, CP_UTF8), CP_ACP);
	}

}

namespace native
{
	struct http_data
	{
		HINTERNET h_session = 0;
		HINTERNET h_connection = 0;
		HINTERNET h_request = 0;
		HANDLE h_file = 0;
		std::vector<char>* mem_data = nullptr;
		bool init(std::wstring u, std::wstring p, int64_t& out_size)
		{
			mem_data = nullptr;
			URL_COMPONENTSW uc = { 0 };
			wchar_t* u_scheme = new wchar_t[64];
			wchar_t* u_host = new wchar_t[512];
			wchar_t* u_path = new wchar_t[8192];
			uc.dwStructSize = sizeof(uc);
			uc.dwSchemeLength = 64;
			uc.dwHostNameLength = 512;
			uc.dwUrlPathLength = 8192;

			uc.lpszScheme = u_scheme;
			uc.lpszHostName = u_host;
			uc.lpszUrlPath = u_path;

			DWORD csz = 0, dwsz = sizeof(dwsz), dwi = 0;
			BOOL bResults = true;

			bResults = WinHttpCrackUrl(u.c_str(), u.size(), ICU_ESCAPE, &uc);
			if (bResults)
				h_connection = WinHttpConnect(h_session, uc.lpszHostName,
					INTERNET_DEFAULT_HTTPS_PORT, 0);

			// Create an HTTP request handle.
			// 这边如果要请求具体某个地址的话 把NULL 改成 XXX地址
			if (h_connection)
				h_request = WinHttpOpenRequest(h_connection, L"GET", uc.lpszUrlPath,
					L"HTTP/1.1", WINHTTP_NO_REFERER,
					WINHTTP_DEFAULT_ACCEPT_TYPES,
					WINHTTP_FLAG_SECURE);
			else bResults = false;


			// Send a request.
			if (h_request)
				bResults = WinHttpSendRequest(h_request,
					WINHTTP_NO_ADDITIONAL_HEADERS,
					0, WINHTTP_NO_REQUEST_DATA, 0,
					0, 0);
			else bResults = false;


			// End the request.
			if (bResults)
				bResults = WinHttpReceiveResponse(h_request, NULL);
			WinHttpQueryHeaders(h_request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &csz, &dwsz, &dwi);
			out_size = csz;
			DWORD code = 0;
			dwsz = sizeof(dwsz);
			WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
				&code, &dwsz, WINHTTP_NO_HEADER_INDEX);
			if (code > 300 || code < 200) bResults = false;
			if (bResults)
				h_file = CreateFileW(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (h_file == 0 || h_file == INVALID_HANDLE_VALUE) bResults = false;
			if (!bResults)
			{
				if (h_connection) WinHttpCloseHandle(h_connection);
				if (h_request) WinHttpCloseHandle(h_request);
				if (h_file) CloseHandle(h_file);
			}
			return bResults;
		}
		bool init(std::wstring u, std::vector<char>* mem, int64_t& out_size)
		{
			mem_data = mem;
			URL_COMPONENTSW uc = { 0 };
			wchar_t* u_scheme = new wchar_t[64];
			wchar_t* u_host = new wchar_t[512];
			wchar_t* u_path = new wchar_t[8192];
			uc.dwStructSize = sizeof(uc);
			uc.dwSchemeLength = 64;
			uc.dwHostNameLength = 512;
			uc.dwUrlPathLength = 8192;

			uc.lpszScheme = u_scheme;
			uc.lpszHostName = u_host;
			uc.lpszUrlPath = u_path;

			DWORD csz = 0, dwsz = sizeof(dwsz), dwi = 0;
			BOOL bResults = true;

			bResults = WinHttpCrackUrl(u.c_str(), u.size(), ICU_ESCAPE, &uc);
			if (bResults)
				h_connection = WinHttpConnect(h_session, uc.lpszHostName,
					uc.nPort, 0);

			// Create an HTTP request handle.
			// 这边如果要请求具体某个地址的话 把NULL 改成 XXX地址
			if (h_connection)
				h_request = WinHttpOpenRequest(h_connection, L"GET", uc.lpszUrlPath,
					L"HTTP/1.1", WINHTTP_NO_REFERER,
					WINHTTP_DEFAULT_ACCEPT_TYPES,
					WINHTTP_FLAG_SECURE);
			else bResults = false;


			// Send a request.
			if (h_request)
				bResults = WinHttpSendRequest(h_request,
					WINHTTP_NO_ADDITIONAL_HEADERS,
					0, WINHTTP_NO_REQUEST_DATA, 0,
					0, 0);
			else bResults = false;


			// End the request.
			if (bResults)
				bResults = WinHttpReceiveResponse(h_request, NULL);
			WinHttpQueryHeaders(h_request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &csz, &dwsz, &dwi);
			DWORD code = 0;
			dwsz = sizeof(dwsz);
			WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
				&code, &dwsz, WINHTTP_NO_HEADER_INDEX);
			if (code > 300 || code < 200) bResults = false;
			if (!bResults)
			{
				if (h_connection) WinHttpCloseHandle(h_connection);
				if (h_request) WinHttpCloseHandle(h_request);
				if (h_file) CloseHandle(h_file);
			}
			out_size = csz;
			h_file = 0;
			return bResults;
		}
		bool iterate(int64_t& new_size, bool& error)
		{
			// Check for available data.
			DWORD dwSize = 0, dwDownloaded = 0;
			if (!WinHttpQueryDataAvailable(h_request, &dwSize))
			{
				error = GetLastError();
				return true;
			}

			if (dwSize)
			{
				// Allocate space for the buffer.
				BYTE* pszOutBuffer = new BYTE[dwSize];
				if (!pszOutBuffer)
				{
					error = ERROR_OUTOFMEMORY;
					dwSize = 0;
					return true;
				}
				else
				{
					if (!WinHttpReadData(h_request, (LPVOID)pszOutBuffer,
						dwSize, &dwDownloaded))
					{
						error = GetLastError();
						delete[] pszOutBuffer;
						return true;
					}
					else
						new_size = dwSize;
					if (h_file)
						WriteFile(h_file, pszOutBuffer, dwSize, nullptr, nullptr);
					else
					{
						auto offs = mem_data->size();
						mem_data->insert(mem_data->end(), dwSize, 0);
						memcpy(&mem_data->operator[](offs), pszOutBuffer, dwSize);
					}
					// Free the memory allocated to the buffer.
					delete[] pszOutBuffer;
				}
			}

			return dwSize <= 0;
		}
		void finalize()
		{
			WinHttpCloseHandle(h_request);
			WinHttpCloseHandle(h_connection);
			if (h_file)
				CloseHandle(h_file);
			h_connection = 0;
			h_request = 0;
			h_file = 0;
		}

		http_data()
		{
			// Use WinHttpOpen to obtain a session handle.
			h_session = WinHttpOpen(L"GMCL Getter/1.0",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0);
		}
		~http_data()
		{
			WinHttpCloseHandle(h_session);
		}
	};
	download_pool::download_pool(int pool_size)
	{
		this->pool_size = pool_size;
		workers.resize(pool_size);
		for (int i = 0; i < pool_size; ++i)
		{
			workers[i].data = new http_data;
			workers[i].thread = std::thread([&, i]()
				{
					this->worker_work(i);
				});
		}
		dispatcher = std::thread([&]()
			{
				this->dispatch();
			});
	}
	download_pool::~download_pool()
	{
		alive = false;
		for (int i = 0; i < pool_size; ++i)
		{
			ResumeThread(workers[i].thread.native_handle());
			if (std::this_thread::get_id() != workers[i].thread.get_id())
				workers[i].thread.join();
			else
				workers[i].thread.detach();
		}
		dispatcher.join();
	}
	void download_pool::dispatch()
	{
		int loop_counter = 0;
		while (alive)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(125));
			if (loop_counter >= 3)
			{
				loop_counter = 0;
				bps = bps_counter.load() * 2;
				bps_counter.store(0);
				//std::cout << "ByteRate: " << byte_per_sec() / 1024.f << "KB/s" << std::endl;
			}
			loop_counter++;
			{
				std::lock_guard guard(mut);
				if (waiting_num == 0 || working_num >= pool_size) continue;
				auto ti = tasks.begin();
				auto wi = workers.begin();
				for (; wi != workers.end() && ti != tasks.end(); )
				{
					if (ti->status == task_status::waiting)
					{
						for (; wi != workers.end(); wi++)
						{
							if (wi->idle)
							{
								wi->task_id = ti - tasks.begin();
								wi->total_size = 0;
								wi->downloaded_size = 0;
								wi->url = ti->url;
								wi->path = ti->file;
								wi->idle = false;
								if (wi->path.length())
								{
									auto dir = fs::path(ti->file).remove_filename();
									if (!dir.empty() && !fs::directory_entry(dir).exists())
									{
										fs::create_directories(dir);
									}
								}
								ti->status = task_status::working;
								ti->worker_id = wi - workers.begin();
								//std::wcout << T("Download begin: ") << ti->user_id << std::endl;
								ResumeThread(wi->thread.native_handle());
								working_num++;
								waiting_num--;
								++wi;
								break;
							}
						}
					}
					ti++;
				}

			}
		}
	}
	void download_pool::worker_work(int id)
	{
		HANDLE thread_handle = workers[id].thread.native_handle();
		SuspendThread(thread_handle);
		std::vector<char> mem_data;
		while (alive)
		{
			worker& w = workers[id];
			if (w.task_id > -1)
			{
				bool error = false;
				int64_t tsz = 0;
				task_status final_status = task_status::finished;
				bool write_file = false;
				bool inited = false;
				if (w.path.length())
					inited = w.data->init(sutil::s2w(w.url), sutil::s2w(w.path), tsz);
				else
					inited = w.data->init(sutil::s2w(w.url), &mem_data, tsz);
				if (inited)
				{
					w.total_size = tsz;
					{
						std::lock_guard guard(mut);
						total_tasks_size += tsz - tasks[w.task_id].size;
					}
					int64_t nsz = 0;
					while (!w.data->iterate(nsz, error))
					{
						w.downloaded_size += nsz;
						bps_counter += nsz;
						received_bytes += nsz;
					}
					if (error)
						final_status = task_status::error;
					w.data->finalize();
				}
				else
					final_status = task_status::error;
				download_task cbt;
				{
					std::lock_guard guard(mut);
					tasks[w.task_id].status = final_status;
					cbt.id = tasks[w.task_id].user_id;
					cbt.url = tasks[w.task_id].url;
					cbt.size = w.downloaded_size;
					if (w.path.length())
					{
						cbt.file = tasks[w.task_id].file;
						cbt.data = nullptr;
					}
					else if (mem_data.size())
						cbt.data = &mem_data[0];
					tasks[w.task_id].status = task_status::empty;
				}
				if (tasks[w.task_id].cb)
					tasks[w.task_id].cb(final_status, &cbt);
				//if (callback)
				//	callback(final_status, cbt);
				mem_data.clear();

				working_num--;
				w.idle = true;
				SuspendThread((HANDLE)thread_handle);
			}
		}
	}

	int64_t download_pool::join_tasks(std::vector<download_task>&& tasks)
	{
		std::lock_guard guard(mut);
		auto ti = this->tasks.begin();
		for (auto& i : tasks)
		{
			task t;
			bool replaced = false;
			t.file = i.file;
			t.url = i.url;
			t.worker_id = -1;
			t.status = task_status::waiting;
			t.user_id = i.id;
			t.size = i.size;
			t.cb = i.callbak;
			total_tasks_size += i.size;
			for (; ti != this->tasks.end();)
			{
				if (ti->status == task_status::empty)
				{
					*ti = t;
					replaced = true;
					++ti;
					break;
				}
				++ti;
			}
			if (!replaced)
				this->tasks.push_back(t);
			waiting_num++;
		}
		return tasks.size();
	}
	int64_t download_pool::join_task(download_task* i)
	{
		std::lock_guard guard(mut);
		auto ti = this->tasks.begin();
		task t;
		bool replaced = false;
		t.file = i->file;
		t.url = i->url;
		t.worker_id = -1;
		t.status = task_status::waiting;
		t.user_id = i->id;
		t.size = i->size;
		t.cb = i->callbak;
		total_tasks_size += i->size;
		for (; ti != this->tasks.end();)
		{
			if (ti->status == task_status::empty)
			{
				*ti = t;
				replaced = true;
				break;
			}
			++ti;
		}
		waiting_num++;
		if (!replaced)
		{
			this->tasks.push_back(t);
			return tasks.size();
		}
		return ti - tasks.begin();
	}
	int64_t download_pool::byte_per_sec()
	{
		return bps;
		//return byterate * (dispacher_sleep / 1000.0);
	}

	int64_t download_pool::received_size()
	{
		return received_bytes;
	}
}