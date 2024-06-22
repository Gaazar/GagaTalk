#include <thread>
#include "trayicon.h"
#include <iostream>
#include "resource.h"
#include <shellapi.h>
#include "events.h"
#include "native_util.hpp"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <fmt/xchar.h>
#include "web.h"

using namespace std;

UINT const WM_APP_NOTIFYCALLBACK = WM_APP + 1;
class __declspec(uuid("618CA4D0-E67F-436F-B6DE-78994711EE4E")) TrayIcon;

#define IDM_CONFIG 0x4000
#define IDM_SERVER 0x8000
#define IDM_CHANNEL 0x10000
#define IDM_CLIENT 0x20000
#define IDM_AUDIN 0x40000
#define IDM_AUDOUT 0x80000
#define IDM_EXIT 0x300
#define IDM_EXIT_CONFIRM 0x301
#define IDM_MUTE 0x302
#define IDM_SILENT 0x303
#define IDM_WEBGUI 0x304

#define IDM_VOLUME_0    0x10'0000
#define IDM_VOLUME_25   0x20'0000
#define IDM_VOLUME_50   0x40'0000
#define IDM_VOLUME_75   0x80'0000
#define IDM_VOLUME_100  0x100'0000
#define IDM_VOLUME_UP   0x200'0000
#define IDM_VOLUME_DOWN 0x400'0000

thread tr_th_msg;
HINSTANCE tr_hinst = 0;
HWND tr_hwnd = 0;
HICON tr_nic = 0;
std::vector<audio_device> tr_ai;
std::vector<audio_device> tr_ao;

bool exit_confirm = false;

int tray_init()
{
	exit_confirm = conf_get_exit_check();
	tr_th_msg = thread(thread_message);
	return 0;
}
int tray_uninit()
{
	PostMessage(tr_hwnd, WM_DESTROY, 0, 0);
	DestroyWindow(tr_hwnd);
	tr_th_msg.detach();
	return 0;
}
void RegisterWindowClass(PCWSTR pszClassName, PCWSTR pszMenuName, WNDPROC lpfnWndProc)
{
	WNDCLASSEX wcex = { sizeof(wcex) };
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = lpfnWndProc;
	wcex.hInstance = tr_hinst;
	wcex.hIcon = LoadIcon(tr_hinst, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = pszMenuName;
	wcex.lpszClassName = pszClassName;
	RegisterClassEx(&wcex);
}

BOOL AddNotificationIcon(HWND hwnd)
{
	if (!tr_nic)
		tr_nic = (HICON)LoadImage(tr_hinst, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);;
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.uID = (UINT)hwnd;
	nid.hWnd = hwnd;
	// add the icon, setting the icon, tooltip, and callback message.
	// the icon will be identified with the GUID
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP /*| NIF_GUID*/;
	//nid.guidItem = __uuidof(TrayIcon);
	nid.uCallbackMessage = WM_APP_NOTIFYCALLBACK;
	nid.hIcon = tr_nic;
	//LoadString(tr_hinst, IDS_TOOLTIP, nid.szTip, ARRAYSIZE(nid.szTip));
	wcscpy(nid.szTip, L"GagaTalk\n未连接服务器");
	Shell_NotifyIcon(NIM_ADD, &nid);

	// NOTIFYICON_VERSION_4 is prefered
	nid.uVersion = NOTIFYICON_VERSION_4;
	return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}
BOOL ModifyNotificationIcon(HWND hwnd, std::string info)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.uID = (UINT)hwnd;
	nid.hWnd = hwnd;
	// add the icon, setting the icon, tooltip, and callback message.
	// the icon will be identified with the GUID
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP /*| NIF_GUID*/;
	//nid.guidItem = __uuidof(TrayIcon);
	nid.uCallbackMessage = WM_APP_NOTIFYCALLBACK;
	nid.hIcon = tr_nic;
	wcscpy(nid.szTip, sutil::s2w(fmt::format("Gagatalk\n{}", info)).c_str());
	return Shell_NotifyIcon(NIM_MODIFY, &nid);
}
BOOL DeleteNotificationIcon()
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = tr_hwnd;
	nid.uID = (UINT)tr_hwnd;
	//nid.uFlags = NIF_GUID;
	//nid.guidItem = __uuidof(TrayIcon);
	return Shell_NotifyIcon(NIM_DELETE, &nid);
}

std::vector<server_info> tr_svrs;
connection* tr_conn = nullptr;
std::vector<suid_t> volume_list;
void FreeMenu();
struct MenuState
{
	HMENU root = 0;
	HMENU svr = 0;
	HMENU chan = 0;
	HMENU conf = 0;
	HMENU auddev = 0;
	std::vector<HMENU> ents;
};
MenuState tr_menu;
HMENU MakeMenu()
{
	if (tr_menu.root)
	{
		FreeMenu();
	}
	tr_menu.root = CreatePopupMenu();
	tr_menu.svr = CreatePopupMenu();
	tr_menu.chan = CreatePopupMenu();
	tr_menu.conf = CreatePopupMenu();
	tr_menu.auddev = CreatePopupMenu();

	tr_conn = client_get_current_server();
	conf_get_servers(tr_svrs);
	if (tr_svrs.size())
	{
		int n = 0;
		for (auto& i : tr_svrs)
		{
			if (tr_conn && tr_conn->host == i.host && tr_conn->status != connection::state::disconnect)
				AppendMenu(tr_menu.svr, MF_STRING | MF_CHECKED, IDM_SERVER | n, sutil::s2w(i.name).c_str());
			else
				AppendMenu(tr_menu.svr, MF_STRING, IDM_SERVER | n, sutil::s2w(i.name).c_str());
			n++;
		}
	}
	else AppendMenu(tr_menu.svr, MF_STRING | MF_DISABLED, 0, TEXT("无"));
	if (tr_conn && tr_conn->channels.size() > 0)
	{
		int n = 0;
		for (auto& i : tr_conn->channels)
		{
			if (tr_conn->current == i.second)
				AppendMenu(tr_menu.chan, MF_STRING | MF_CHECKED, 0, sutil::s2w(i.second->name).c_str());
			else
				AppendMenu(tr_menu.chan, MF_STRING, IDM_CHANNEL | n, sutil::s2w(i.second->name).c_str());
			n++;
		}
	}
	else  AppendMenu(tr_menu.chan, MF_STRING | MF_DISABLED, 0, TEXT("无"));

	//AppendMenu(trm_conf, MF_STRING | MF_DISABLED, 0, TEXT("无"));

	if (tr_conn && tr_conn->status == connection::state::established && tr_conn->current)
	{
		int n = 0;
		AppendMenu(tr_menu.root, MF_STRING, 0, fmt::format(L"频道: {}", sutil::s2w(tr_conn->current->name)).c_str());

		for (auto& i : tr_conn->current->entities)
		{
			if (i->playback)
			{
				HMENU ent_v = CreatePopupMenu();
				tr_menu.ents.push_back(ent_v);
				volume_list.push_back(i->suid);
				AppendMenu(ent_v, MF_STRING | MF_DISABLED, 0, (L"音量:" + std::to_wstring(i->get_volume()) + L"dB").c_str());
				AppendMenu(ent_v, MF_STRING | (i->get_volume() == percent_to_db(0) ? MF_CHECKED : 0), IDM_VOLUME_0 | n, TEXT("0%"));
				AppendMenu(ent_v, MF_STRING | (i->get_volume() == percent_to_db(0.25f) ? MF_CHECKED : 0), IDM_VOLUME_25 | n, TEXT("25%"));
				AppendMenu(ent_v, MF_STRING | (i->get_volume() == percent_to_db(0.50f) ? MF_CHECKED : 0), IDM_VOLUME_50 | n, TEXT("50%"));
				AppendMenu(ent_v, MF_STRING | (i->get_volume() == percent_to_db(0.75f) ? MF_CHECKED : 0), IDM_VOLUME_75 | n, TEXT("75%"));
				AppendMenu(ent_v, MF_STRING | (i->get_volume() == percent_to_db(1.00f) ? MF_CHECKED : 0), IDM_VOLUME_100 | n, TEXT("100%"));
				AppendMenu(ent_v, MF_STRING, IDM_VOLUME_UP | n, TEXT("音量加"));
				AppendMenu(ent_v, MF_STRING, IDM_VOLUME_DOWN | n, TEXT("音量减"));
				AppendMenu(tr_menu.root, MF_POPUP, (UINT_PTR)ent_v, sutil::s2w(i->name).c_str());
				n++;
			}
			else
				AppendMenu(tr_menu.root, MF_STRING | MF_DISABLED, IDM_CLIENT | n, (sutil::s2w(i->name) + L"\t自己").c_str());
		}
	}
	plat_enum_input_device(tr_ai);
	AppendMenu(tr_menu.auddev, MF_STRING | MF_DISABLED, 0, TEXT("输入"));
	for (int i = 0; i < tr_ai.size(); i++)
	{
		AppendMenu(tr_menu.auddev, MF_STRING | ((plat_get_input_device() == tr_ai[i].id) ? MF_CHECKED : MF_UNCHECKED), IDM_AUDIN | i, sutil::s2w(tr_ai[i].name).c_str());
	}
	AppendMenu(tr_menu.auddev, MF_SEPARATOR, 0, NULL);
	AppendMenu(tr_menu.auddev, MF_STRING | MF_DISABLED, 0, TEXT("输出"));
	plat_enum_output_device(tr_ao);
	for (int i = 0; i < tr_ao.size(); i++)
	{
		AppendMenu(tr_menu.auddev, MF_STRING | ((plat_get_output_device() == tr_ao[i].id) ? MF_CHECKED : MF_UNCHECKED), IDM_AUDOUT | i, sutil::s2w(tr_ao[i].name).c_str());
	}
	AppendMenu(tr_menu.conf, MF_POPUP, (UINT_PTR)tr_menu.auddev, TEXT("输入输出"));


	AppendMenu(tr_menu.root, MF_SEPARATOR, 0, NULL);
	AppendMenu(tr_menu.root, MF_POPUP, (UINT_PTR)tr_menu.conf, TEXT("设置"));
	AppendMenu(tr_menu.root, MF_SEPARATOR, 0, NULL);
	AppendMenu(tr_menu.root, MF_POPUP, (UINT_PTR)tr_menu.svr, TEXT("服务器"));
	AppendMenu(tr_menu.root, MF_POPUP, (UINT_PTR)tr_menu.chan, TEXT("频道"));
	AppendMenu(tr_menu.root, MF_SEPARATOR, 0, NULL);
	AppendMenu(tr_menu.root, MF_STRING
		| (plat_get_global_mute() ? MF_CHECKED : MF_UNCHECKED)
		| (tr_conn ? (tr_conn->entity_state.man_mute ? MF_DISABLED : 0) : 0), IDM_MUTE, TEXT("闭麦\tCtrl+Alt+M"));
	AppendMenu(tr_menu.root, MF_STRING
		| (plat_get_global_silent() ? MF_CHECKED : MF_UNCHECKED)
		| (tr_conn ? (tr_conn->entity_state.man_silent ? MF_DISABLED : 0) : 0), IDM_SILENT, TEXT("静音\tCtrl+Alt+S"));
	AppendMenu(tr_menu.root, MF_STRING, IDM_WEBGUI, TEXT("打开主界面"));
	AppendMenu(tr_menu.root, MF_SEPARATOR, 0, NULL);
	AppendMenu(tr_menu.root, MF_STRING | (exit_confirm ? MF_CHECKED : MF_UNCHECKED), IDM_EXIT_CONFIRM, TEXT("确认退出"));
	AppendMenu(tr_menu.root, MF_STRING, IDM_EXIT, TEXT("退出"));

	return tr_menu.root;

}
void FreeMenu()
{
	DestroyMenu(tr_menu.root);
	DestroyMenu(tr_menu.svr);
	DestroyMenu(tr_menu.chan);
	DestroyMenu(tr_menu.conf);
	DestroyMenu(tr_menu.auddev);
	for (auto& i : tr_menu.ents)
	{
		DestroyMenu(i);
	}
	tr_menu.ents.clear();
	volume_list.clear();
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HWND s_hwndFlyout = NULL;
	static BOOL s_fCanShowFlyout = TRUE;

	switch (message)
	{
	case WM_CREATE:
		// add the notification icon
		if (!AddNotificationIcon(hwnd))
		{
			MessageBox(hwnd,
				L"Create Icon helper window error",
				L"Error adding icon", MB_OK);
			return -1;
		}
		break;
	case WM_COMMAND:
	{
		UINT64 const wmId = wParam;
		auto c = client_get_current_server();
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_EXIT_CONFIRM:
			exit_confirm = !exit_confirm;
			conf_set_exit_check(exit_confirm);
			break;
		case IDM_EXIT:
			if (exit_confirm)
			{
				client_uninit();
				exit(0);
			}
			else
				exit_confirm = true;
			break;
		case IDM_MUTE:
		{
			bool m = !plat_get_global_mute();
			client_set_global_mute(m);
		}
		break;
		case IDM_SILENT:
		{
			bool s = !plat_get_global_silent();
			client_set_global_silent(s);
		}
		break;
		case IDM_WEBGUI:
			web_open_browser("http://127.0.0.1:10709");
			break;
		default:
			if (wmId & IDM_CLIENT)
			{
				int n = ~IDM_CLIENT & wmId;
				break;
			}
			else if (wmId & IDM_SERVER)
			{
				int n = ~IDM_SERVER & wmId;
				if (c && c->status == connection::state::established)
				{
					client_disconnect(c);
				}
				client_connect(tr_svrs[n].host, 7970, &c);
				break;
			}
			else if (wmId & IDM_CHANNEL)
			{
				int n = ~IDM_CHANNEL & wmId;
				if (c)
				{
					int j = 0;
					for (auto i = c->channels.begin(); i != c->channels.end(); ++i)
					{
						if (n == j)
						{
							c->join_channel(i->second->chid);
							break;
						}
						j++;
					}
				}
				break;
			}
			else if (wmId & IDM_AUDIN)
			{
				int n = ~IDM_AUDIN & wmId;
				bool s = plat_set_input_device(tr_ai[n].id);
				conf_set_input_device(&tr_ai[n].id);

			}
			else if (wmId & IDM_AUDOUT)
			{
				int n = ~IDM_AUDOUT & wmId;
				bool s = plat_set_output_device(tr_ao[n].id);
				conf_set_output_device(&tr_ao[n].id);
			}
			else if (wmId & IDM_VOLUME_0)
			{
				suid_t s = volume_list[~IDM_VOLUME_0 & wmId];
				c->set_client_volume(s, percent_to_db(0));
			}
			else if (wmId & IDM_VOLUME_25)
			{
				suid_t s = volume_list[~IDM_VOLUME_25 & wmId];
				c->set_client_volume(s, percent_to_db(.25f));
			}
			else if (wmId & IDM_VOLUME_50)
			{
				suid_t s = volume_list[~IDM_VOLUME_50 & wmId];
				c->set_client_volume(s, percent_to_db(.50f));
			}
			else if (wmId & IDM_VOLUME_75)
			{
				suid_t s = volume_list[~IDM_VOLUME_75 & wmId];
				c->set_client_volume(s, percent_to_db(.75f));
			}
			else if (wmId & IDM_VOLUME_100)
			{
				suid_t s = volume_list[~IDM_VOLUME_100 & wmId];
				c->set_client_volume(s, percent_to_db(1));
			}
			else if (wmId & IDM_VOLUME_UP)
			{
				suid_t s = volume_list[~IDM_VOLUME_UP & wmId];
				c->set_client_volume(s, c->get_client_volume(s) + 2);
			}
			else if (wmId & IDM_VOLUME_DOWN)
			{
				suid_t s = volume_list[~IDM_VOLUME_DOWN & wmId];
				c->set_client_volume(s, c->get_client_volume(s) - 2);
			}
			else
				return DefWindowProc(hwnd, message, wParam, lParam);
		}
	}
	break;

	case WM_APP_NOTIFYCALLBACK:
		switch (LOWORD(lParam))
		{
		case NIN_SELECT:
			// for NOTIFYICON_VERSION_4 clients, NIN_SELECT is prerable to listening to mouse clicks and key presses
			// directly.
			break;

		case WM_CONTEXTMENU:
		{
			POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
			SetForegroundWindow(hwnd);
			auto hMenu = MakeMenu();
			TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, NULL, hwnd, NULL);
			//ShowContextMenu(hwnd, pt);
		}
		break;
		case WM_LBUTTONUP:
			web_open_browser("http://127.0.0.1:10709");

			break;
		}
		break;

	case WM_DESTROY:
		DeleteNotificationIcon();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

void thread_message()
{

	tr_hinst = GetModuleHandle(NULL);
	RegisterWindowClass(L"GagaTalk", NULL, WndProc);
	HWND hwnd = CreateWindow(L"GagaTalk", L"嘎嘎乱喊", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 250, 200, NULL, NULL, tr_hinst, NULL);
	tr_hwnd = hwnd;
	e_channel += [](event t, channel* e)
		{
			if (t == event::join)
			{
				auto b = ModifyNotificationIcon(tr_hwnd, fmt::format("服务器：{}\n频道：{}", e->conn->name, e->name));
			}
		};

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}