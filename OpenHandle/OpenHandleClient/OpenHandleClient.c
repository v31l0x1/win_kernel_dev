#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <Psapi.h>

#define IOCTL_OPEN_PROC_HANDLE	CTL_CODE(0x8000, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
	DWORD pid;
	HANDLE hProcess;
} OPEN_PROC_HANDLE_DATA;

BOOL ListDlls(HANDLE hProcess) {
	HMODULE hMods[1024];
	DWORD cbNeeded;
	if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
		return FALSE;
	}
	
	int count = cbNeeded / sizeof(HMODULE);
	WCHAR modName[MAX_PATH];
	for (int i = 0; i < count; i++) {	
		GetModuleBaseName(hProcess, hMods[i], modName, _countof(modName));
		wprintf(L"0x%p: %ls\n", hMods[i], modName);
	}

	return TRUE;
}

int main(int argc, char* argv[]) {

	if (argc < 2) {
		printf("[+] Usage: %s <pid>\n", argv[0]);
		return 1;
	}

	DWORD pid = atoi(argv[1]);

	HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);

	if (!hProcess) {
		HANDLE hDevice = CreateFile(L"\\\\.\\OpenHandle",
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (hDevice != INVALID_HANDLE_VALUE) {
			DWORD bytesReturned = 0;
			
			OPEN_PROC_HANDLE_DATA data = { 0 };
			data.pid = pid;
		
			BOOL success = DeviceIoControl(hDevice, 
				IOCTL_OPEN_PROC_HANDLE, 
				&data, 
				sizeof(data), 
				&data, 
				sizeof(data), 
				&bytesReturned, 
				NULL
			);

			CloseHandle(hDevice);

			if (success) {
				printf("[+] Process handle: 0x%p\n", data.hProcess);
				hProcess = data.hProcess;
			} else {
				printf("[-] DeviceIoControl failed with error: %lu\n", GetLastError());
			}
		}
		else {
			printf("[-] CreateFile failed with error: %lu\n", GetLastError());
		}
	}

	if (hProcess && hProcess != INVALID_HANDLE_VALUE) {
		ListDlls(hProcess);
		CloseHandle(hProcess);
	}
	else {
		printf("[-] Error: %lu\n", GetLastError());
	}
	return 0;

}