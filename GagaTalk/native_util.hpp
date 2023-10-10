#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <functional>
namespace sutil
{
	std::string w2s(const std::wstring& wstr, unsigned int code_page = 0);

	std::wstring s2w(const std::string& str, unsigned int code_page = 0);
	std::wstring s2w(const char* str, size_t len);

	//inline std::vector<string> split(string& s, wchar_t spliter = ' ')
	//{
	//	std::vector<string> result;
	//	auto sz = s.size();
	//	size_t b = 0;
	//	for (auto i = 0; i < sz; ++i)
	//	{
	//		if (s[i] == spliter)
	//		{
	//			result.push_back(s.substr(b, i - b));
	//			b = ++i;
	//		}
	//	}
	//	result.push_back(s.substr(b, sz - b));
	//	return result;
	//}

	//inline string escape(const string& s)
	//{
	//	if (s.find(' ') == std::wstring::npos)
	//	{
	//		return s;
	//	}
	//	return L"\"" + s + L"\"";
	//}

	//inline bool version_compare(string& newer, string& older, wchar_t spliter = L'.')
	//{
	//	auto n = split(newer, spliter);
	//	auto o = split(newer, spliter);
	//	if (n.size() != o.size())
	//		return n.size() >= o.size();
	//	auto m = std::min(n.size(), o.size());
	//	for (auto i = 0; i < m; i++)
	//	{
	//		if (wcstol(n[i].c_str(), 0, 10) < wcstol(o[i].c_str(), 0, 10))
	//		{
	//			return false;
	//		}
	//	}
	//	return true;
	//}


}

namespace native
{
	//enum class native_error
	//{
	//	no = 0,
	//	wait_abandoned,
	//	wait_failed,
	//	wait_timeout,
	//	null_arg,
	//	unknown,
	//	version_file_borken
	//};
	//bool excute(int64_t* exit_code, int64_t* process, const gmcl::string& cmd, const gmcl::string& work_dir);
	//native_error wait_for_exit(int64_t process, int64_t* exit_code);
	//native_error extract_natives(std::vector<gmcl::native_lib_desc>& natives, gmcl::string target_dir);

	struct http_data;
	struct download_task;

	class download_pool
	{
	public:
		struct worker
		{
			std::thread thread;
			bool idle = true;
			int64_t total_size;
			int64_t downloaded_size;
			std::string url;
			std::string path;
			int task_id;
			http_data* data;
		};
		enum class task_status
		{
			waiting = 0,
			working,
			finished,
			empty,
			error
		};
		typedef std::function<void(download_pool::task_status s, download_task* t)> callback;
		struct task
		{
			std::string url;
			std::string file;
			std::string user_id;
			callback cb;
			task_status status = task_status::waiting;
			int worker_id = -1;
			int64_t size;
		};
	private:
		int32_t working_num = 0;
		int32_t waiting_num = 0;
		std::vector<worker> workers;
		std::thread dispatcher;
		std::mutex mut;
		bool alive = true;
		std::vector<task> tasks;
		std::atomic<int32_t> bps_counter = 0;//
		std::atomic<int64_t> total_tasks_size = 0;
		std::atomic<int64_t> received_bytes = 0;
		int32_t bps;

		//std::function<bool(task_status s, download_task& t)> callback;
	public:
		int pool_size = 8;
		download_pool(int pool_size = 8);
		int64_t join_tasks(std::vector<download_task>&& tasks);
		int64_t join_task(download_task* task);
		bool abort_task(int64_t task);
		~download_pool();

		void dispatch();
		void worker_work(int id);
		int64_t byte_per_sec();
		int64_t received_size();
		/// <summary>
		/// return true if wants to reuse the taskid
		/// </summary>
	};

	struct download_task
	{
		std::string url;
		std::string file;
		std::string id;
		int64_t size = 0;
		char* data = nullptr;
		download_pool::callback callbak;
		download_task() {};
		download_task(std::string url, std::string file, download_pool::callback cb = download_pool::callback(), std::string id = "Download Task", int64_t est_size = 0)
		{
			this->url = url;
			this->file = file;
			this->id = id;
			this->size = est_size;
			this->callbak = cb;
		}
		download_task(std::string url, download_pool::callback cb = download_pool::callback(), std::string id = "HTTP Task", int64_t est_size = 0)
		{
			this->url = url;
			this->id = id;
			this->size = est_size;
			this->callbak = cb;
		}
	};

}