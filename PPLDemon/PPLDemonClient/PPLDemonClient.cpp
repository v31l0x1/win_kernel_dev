#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"

#define IOCTL_RM_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ADD_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _Protection {
	ULONG ProcessId;
} Protection, * PProtection;

DWORD findProc(LPCWSTR processName) {

	DWORD pid = 0;

	HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
	if (!hNtdll) {
		wprintf(L"[-] Failed to get handle to ntdll.dll\n");
		return 0;
	}

	fnNtQuerySystemInformation NtQuerySystemInformation = (fnNtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
	if (!NtQuerySystemInformation) {
		wprintf(L"[-] Failed to get address of NtQuerySystemInformation\n");
		return 0;
	}

	NTSTATUS status;
	ULONG returnLength = 0;
	status = NtQuerySystemInformation(SystemProcessInformation, NULL, 0, &returnLength);

	if (status != STATUS_INFO_LENGTH_MISMATCH) {
		wprintf(L"[-] NtQuerySystemInformation failed to get required buffer size\n");
		return 0;
	}

	PVOID buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, returnLength);
	if (!buffer) {
		wprintf(L"[-] Failed to allocate memory for process information\n");
		return 0;
	}

	status = NtQuerySystemInformation(SystemProcessInformation, buffer, returnLength, &returnLength);

	if (!NT_SUCCESS(status)) {
		wprintf(L"[-] NtQuerySystemInformation failed\n");
		HeapFree(GetProcessHeap(), 0, buffer);
		return 0;
	}

	PSYSTEM_PROCESS_INFORMATION pInfo = (PSYSTEM_PROCESS_INFORMATION)buffer;
	while (TRUE) {
		if (pInfo->ImageName.Buffer && _wcsicmp(pInfo->ImageName.Buffer, processName) == 0) {
			pid = (DWORD)(ULONG_PTR)pInfo->UniqueProcessId;
			break;
		}

		if (pInfo->NextEntryOffset == 0) {
			break;
		}

		pInfo = (PSYSTEM_PROCESS_INFORMATION)((PBYTE)pInfo + pInfo->NextEntryOffset);

	}

	HeapFree(GetProcessHeap(), 0, buffer);
	return pid;
}

VOID ModifyPPL(DWORD pid, BOOL add) {
	DWORD Pid = pid;
	DWORD IOCTLCode = add ? IOCTL_ADD_PPL : IOCTL_RM_PPL;
	
	HANDLE hDevice = CreateFile(
		L"\\\\.\\PPLDemon",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		wprintf(L"[-] Failed to open device\n");
		return;
	}

	Protection protection;
	protection.ProcessId = pid;

	DWORD bytesReturned;
	BOOL success = DeviceIoControl(
		hDevice,
		IOCTLCode,
		&protection,
		sizeof(Protection),
		NULL,
		0,
		&bytesReturned,
		NULL
	);

	if (!success) {
		wprintf(L"[-] DeviceIoControl failed\n");
		CloseHandle(hDevice);
		return;
	}

	wprintf(L"[+] Successfully %s PPL for process with PID: %lu\n", add ? "added" : "removed", pid);
}



int main(int argc, wchar_t* argv[]) {

	if (argc != 3) {
		wprintf(L"Usage: %s <process_name> <action>\n", argv[0]);
		return 1;
	}

	if (_wcsicmp(argv[2], L"add") != 0 && _wcsicmp(argv[2], L"remove") != 0) {
		wprintf(L"[-] Invalid action. Use 'add' or 'remove'.\n");
		return 1;
	}

	LPCWSTR processName = argv[1];
	DWORD pid = findProc(processName);

	if (pid == 0) {
		wprintf(L"[-] Process %s not found\n", processName);
		return -1;
	}

	wprintf(L"[+] Found process %ls with PID: %lu\n", processName, pid);

	if (_wcsicmp(argv[2], L"add") == 0) {
		wprintf(L"[+] Adding PPL for process %s\n", argv[1]);
		ModifyPPL(findProc(argv[1]), TRUE);
	}
	else {
		wprintf(L"[+] Removing PPL for process %s\n", argv[1]);
		ModifyPPL(findProc(argv[1]), FALSE);
	}
	
	return 0;
}