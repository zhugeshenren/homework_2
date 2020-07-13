
#include <windows.h>
#include <conio.h>
#include <stdio.h>

#include <thread>
#include <algorithm>
#include <string>
#include <mutex>
#include <tlhelp32.h>//声明快照函数的头文件
#include <stdio.h>
#include <map>
#include <set>
#include <ctime>
#include <iterator>
#include <thread>
#include <algorithm>
#include <string>
#include <mutex>
#include <ostream>
#include <fstream>
#include <iostream>

using namespace std;
// 是不是显示窗口
//#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

mutex mtx;
map<wstring, time_t> map_process;

char kb_str[200];
char app_str[300];

int task();
int get_process_set(set<wstring>* process_set, map<wstring, time_t>* dict);
void  FileTimeToTime_t(FILETIME  ft, time_t* t);



DWORD g_main_tid = 0;
HHOOK g_kb_hook = 0;

int write_string_to_file_append(const std::string& file_string, const std::string str)
{
	std::ofstream OsWrite(file_string, std::ofstream::app);
	OsWrite << str;
	OsWrite.close();
	return 0;
}



//将wstring转换成string  
string wstring2string(wstring wstr)
{
	string result;
	//获取缓冲区大小，并申请空间，缓冲区大小事按字节计算的  
	int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
	char* buffer = new char[len + 1];
	//宽字节编码转换成多字节编码  
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), buffer, len, NULL, NULL);
	buffer[len] = '\0';
	//删除缓冲区并返回值  
	result.append(buffer);
	delete[] buffer;
	return result;
}


bool CALLBACK con_handler(DWORD CtrlType)
{
    printf("count");
    PostThreadMessage(g_main_tid, WM_QUIT, 0, 0);
    return TRUE;
};

LRESULT CALLBACK kb_proc(int code, WPARAM w, LPARAM l)
{
    PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)l;
    const char* info = NULL;
    if (w == WM_KEYDOWN)
        info = "key dn";
    else if (w == WM_KEYUP)
        info = "key up";
    else if (w == WM_SYSKEYDOWN)
        info = "sys key dn";
    else if (w == WM_SYSKEYUP)
        info = "sys key up";

	time_t now = time(0);

    printf("time %d : %s - vkCode [%04x], scanCode [%04x]\n",
        now, info, p->vkCode, p->scanCode);
	// 写入文件
	sprintf_s(kb_str, "time %d : %s - vkCode [%04x], scanCode [%04x]\n", now, info, p->vkCode, p->scanCode);
	write_string_to_file_append(string("key_event.txt"), string(kb_str));
    // 调用下一个hook
    return CallNextHookEx(g_kb_hook, code, w, l);
};

DWORD WINAPI KbHookThread(LPVOID lpParam)
{
    g_main_tid = GetCurrentThreadId();

    
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)&con_handler, TRUE);

	// hook
    g_kb_hook = SetWindowsHookEx(WH_KEYBOARD_LL, 
        (HOOKPROC)&kb_proc, 
        NULL,
        0);

    if (g_kb_hook == NULL) {
        printf("SetWindowsHookEx failed, %ld\n", GetLastError());
        return 0;
    }

    
    MSG msg;


    
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

  
    UnhookWindowsHookEx(g_kb_hook);
    return 0;
};

int main()
{
    // 创建一个键盘事件处理线程
    HANDLE h = CreateThread(NULL, 0, KbHookThread, NULL, 0, NULL);
	auto th = thread(task);

    if (NULL == h) {
        printf("CreateThread failed, %ld\n", GetLastError());
        return -1;
    }

    //阻塞主线程，等待线程退出
    WaitForMultipleObjects(1, &h, TRUE, INFINITE);

	th.join();
    return 0;
}



// 时间转换 unix 时间戳
void  FileTimeToTime_t(FILETIME  ft, time_t* t)
{
	LONGLONG  ll;

	ULARGE_INTEGER ui;
	ui.LowPart = ft.dwLowDateTime;
	ui.HighPart = ft.dwHighDateTime;

	ll = (ft.dwHighDateTime << 32) + ft.dwLowDateTime;

	*t = ((LONGLONG)(ui.QuadPart - 116444736000000000) / 10000000);
}


int get_process_set(set<wstring>* process_set, map<wstring, time_t>* dict) {
	dict->clear();
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(pe32);
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hProcessSnap == INVALID_HANDLE_VALUE)
	{
		cout << "CreateToolhelp32Snapshot调用失败";
		return -1;
	}
	BOOL bMore = ::Process32First(hProcessSnap, &pe32);

	LPFILETIME lpCreationTime = (LPFILETIME)malloc(sizeof(FILETIME));
	LPFILETIME lpExitTime = (LPFILETIME)malloc(sizeof(FILETIME));
	LPFILETIME lpKernelTime = (LPFILETIME)malloc(sizeof(FILETIME));
	LPFILETIME lpUserTime = (LPFILETIME)malloc(sizeof(FILETIME));
	time_t* lpTime = (time_t*)malloc(sizeof(time_t));

	while (bMore)
	{
		wstring t_s = pe32.szExeFile;
		process_set->insert(t_s);

		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, pe32.th32ProcessID);

		GetProcessTimes(hProcess, lpCreationTime, lpExitTime, lpKernelTime, lpUserTime);

		FileTimeToTime_t(*lpCreationTime, lpTime);


		if (*lpTime < 0) {
			*lpTime = 0;
		}

		dict->insert(pair<wstring, time_t>(t_s, *lpTime));

		bMore = ::Process32Next(hProcessSnap, &pe32);
	}
	CloseHandle(hProcessSnap);

	free(lpCreationTime);
	free(lpExitTime);
	free(lpKernelTime);
	free(lpUserTime);
	free(lpTime);


	return 0;
}

int task() {
	set<wstring> set_process_one;
	set<wstring> set_process_two;
	map<wstring, time_t> dict;

	set<wstring>* p_set_1;
	set<wstring>* p_set_2;
	set<wstring>* p_set;

	p_set_1 = &set_process_one;
	p_set_2 = &set_process_two;

	if (get_process_set(&set_process_one, &dict) != 0) {
		return -1;
	}

	mtx.lock();
	for (auto it = set_process_one.begin(); it != set_process_one.end(); it++) {
		map_process.insert(make_pair(*it, dict[*it]));
	}
	mtx.unlock();

	while (true)
	{
		set<wstring> c1, c2;

		p_set_2->clear();
		get_process_set(p_set_2, &dict);

		// 差集
		set_difference(p_set_1->begin(), p_set_1->end(), p_set_2->begin(), p_set_2->end(), inserter(c1, c1.begin()));

		set_difference(p_set_2->begin(), p_set_2->end(), p_set_1->begin(), p_set_1->end(), inserter(c2, c2.begin()));

		mtx.lock();
		for (auto it = c1.begin(); it != c1.end(); it++) {
			auto item = map_process.find(*it);
			if (item != map_process.end()) {
				time_t t = item->second;
				time_t now_time = time(NULL);
				wcout << *it << "已经结束 : 运行时间" << (now_time - t) << "s \n";
			}
			else
			{
				break;
			}

			map_process.erase(*it);
		}

		

		for (auto it = c2.begin(); it != c2.end(); it++) {

			time_t now = time(0);
			sprintf_s(app_str, "begin time %d : ", now);
			string tmp_s = wstring2string(*it);
			cout << app_str + tmp_s + "\n";
			write_string_to_file_append(string("app_time.txt"), app_str + tmp_s + "\n");

			map_process.insert(make_pair(*it, dict[*it]));
		}

		mtx.unlock();

		p_set = p_set_2;
		p_set_2 = p_set_1;
		p_set_1 = p_set;

		// 每100ms执行一次
		Sleep(200);
	}
}