// GagaTalk.cpp : ���ļ����� "main" ����������ִ�н��ڴ˴���ʼ��������
//
#define WIN32
#include <iostream>
#include <stdio.h>

#include "client.h"
#include <windows.h>
#include "DbgHelp.h"

#pragma comment(lib,"opus.lib")
#pragma comment(lib,"avrt.lib")
bool discard = false;
int shell_main();
int main()
{
	WCHAR cwd[512];
	GetCurrentDirectory(512, cwd);
	std::wcout << cwd << std::endl;
	printf("version:%ua, �ڲ�����, �����а汾\n", BUILD_SEQ);
	shell_main();
	return 0;
}

// ���г���: Ctrl + F5 ����� >����ʼִ��(������)���˵�
// ���Գ���: F5 ����� >����ʼ���ԡ��˵�

// ����ʹ�ü���: 
//   1. ʹ�ý��������Դ�������������/�����ļ�
//   2. ʹ���Ŷ���Դ�������������ӵ�Դ�������
//   3. ʹ��������ڲ鿴���������������Ϣ
//   4. ʹ�ô����б��ڲ鿴����
//   5. ת������Ŀ��>���������Դ����µĴ����ļ�����ת������Ŀ��>�����������Խ����д����ļ���ӵ���Ŀ
//   6. ��������Ҫ�ٴδ򿪴���Ŀ����ת�����ļ���>���򿪡�>����Ŀ����ѡ�� .sln �ļ�
