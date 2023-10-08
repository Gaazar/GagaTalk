// Update.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "zip_file.hpp"
#include <filesystem>
#include <exception>
#include <thread>

#include <windows.h>
using namespace miniz_cpp;
namespace fs = std::filesystem;

int main()
{
	std::cout << "GagaUpdate!\nWait for 3s...\n";
	std::this_thread::sleep_for(std::chrono::seconds(1));
	bool checked = true;
	zip_file z("Update.pak");
	if (!fs::is_directory("./temp"))
		fs::create_directory("./temp");
	for (auto& i : z.infolist())
	{
		auto file = fs::path(".\\").append(i.filename).make_preferred();
		std::cout << "更新文件: " << file;

		auto path = fs::path("./").append(i.filename).make_preferred();
		auto fst = fs::status(path);
		if (i.filename.ends_with('/'))
		{
			if (fst.type() != fs::file_type::directory)
			{
				fs::create_directories(path);
			}
		}
		else
		{
			if (i.filename == "Update.exe")
			{
				std::cout << " 成功(update)." << std::endl;
				z.extract(i, fs::path("./temp/").make_preferred().string());
			}
			else
				if (fs::exists(file))
				{
					try
					{
						if (!fs::remove(file))
						{
							throw std::exception("FAILED REMOVE");
						}
						else
						{
							std::cout << " 成功(removed)." << std::endl;
							z.extract(i, fs::path("./").make_preferred().string());
						}
					}
					catch (std::exception e)
					{
						checked = false;
						std::cout << " 失败(using)." << std::endl;
						break;
					}
				}
				else
				{
					std::cout << " 成功(no exist)." << std::endl;
					z.extract(i, fs::path("./").make_preferred().string());
				}
		}

		//fs::remove(fs::path(".\\").append(i.filename));
	}
	z.reset();
	fs::remove("Update.pak");
	if (checked)
	{
		std::cout << "OK!\n";

		//SHELLEXECUTEINFO ShExecInfo = { 0 };
		//ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		//ShExecInfo.fMask = SEE_MASK_DEFAULT;
		//ShExecInfo.hwnd = NULL;
		//ShExecInfo.lpVerb = L"open";
		//ShExecInfo.lpFile = L"GagaTalk.exe";
		//ShExecInfo.lpParameters = L"";
		//ShExecInfo.lpDirectory = NULL;
		//ShExecInfo.nShow = SW_SHOW;
		//ShExecInfo.hInstApp = NULL;
		//ShellExecuteEx(&ShExecInfo);
		BOOL ret = FALSE;
		DWORD u_ExplorerPID;
		HANDLE hTokenUser = 0;
		HANDLE h_Token = 0;
		HANDLE h_Process = 0;
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOWNORMAL;
		HWND h_Progman = GetShellWindow();
		GetWindowThreadProcessId(h_Progman, &u_ExplorerPID);
		h_Process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, u_ExplorerPID);
		OpenProcessToken(h_Process, TOKEN_DUPLICATE, &h_Token);
		DuplicateTokenEx(h_Token, TOKEN_ALL_ACCESS, 0, SecurityImpersonation, TokenPrimary, &hTokenUser);
		ret = CreateProcessWithTokenW(hTokenUser, NULL, L"GagaTalk.exe", (WCHAR*)L"", NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
		if (h_Token) {
			CloseHandle(h_Token);
		}
		if (hTokenUser) {
			CloseHandle(hTokenUser);
		}
		if (h_Process) {
			CloseHandle(h_Process);
		}
	}
	else
	{
		std::cout << "更新失败，请重新安装！\n";
		system("pause");
		return -1;
	}
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
