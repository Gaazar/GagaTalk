// GagaTalk.cpp : ���ļ����� "main" ����������ִ�н��ڴ˴���ʼ��������
//
#define WIN32
#include <iostream>
#include <stdio.h>

#include "client.h"
#include <windows.h>
#include "DbgHelp.h"
#include <sstream>
#include "web.h"

#pragma comment(lib,"opus.lib")
#pragma comment(lib,"avrt.lib")
#pragma comment( lib, "Dbghelp.lib" )

#include <configor/json.hpp>
bool discard = false;
int shell_main();
int GenerateMiniDump(PEXCEPTION_POINTERS pExceptionPointers)
{
	// ���庯��ָ��

	// ���� dmp �ļ���
	TCHAR szFileName[MAX_PATH] = { 0 };
	SYSTEMTIME stLocalTime;
	GetLocalTime(&stLocalTime);
	wsprintf(szFileName, L"gtdumpv%u-%04d%02d%02d-%02d%02d%02d.dmp",
		BUILD_SEQ, stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
		stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond);
	HANDLE hDumpFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	if (INVALID_HANDLE_VALUE == hDumpFile)
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	// д�� dmp �ļ�
	MINIDUMP_EXCEPTION_INFORMATION expParam;
	expParam.ThreadId = GetCurrentThreadId();
	expParam.ExceptionPointers = pExceptionPointers;
	expParam.ClientPointers = FALSE;
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
		hDumpFile, MiniDumpWithDataSegs, (pExceptionPointers ? &expParam : NULL), NULL, NULL);
	// �ͷ��ļ�
	CloseHandle(hDumpFile);
	return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI ExceptionFilter(LPEXCEPTION_POINTERS lpExceptionInfo)
{
	if (IsDebuggerPresent())
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	return GenerateMiniDump(lpExceptionInfo);
}

int main()
{

	SetUnhandledExceptionFilter(ExceptionFilter);

	WCHAR cwd[512];
	GetCurrentDirectory(512, cwd);
	std::wcout << cwd << std::endl;
	printf("version:%ua, �ڲ�����, �����а汾\n", BUILD_SEQ);
	shell_main();
	return 0;
}

