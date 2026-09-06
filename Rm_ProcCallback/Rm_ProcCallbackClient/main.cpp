#include <Windows.h>
#include <stdio.h>
#include <winioctl.h>
#include "defines.h"

#define IOCTL_READ_PROC_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RM_PROC_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define ENTRY_SIZE 56
#define MAX_ENTRIES 64

typedef struct _PROC_CALLBACK_DATA {
	int Index;
} PROC_CALLBACK_DATA, * PPROC_CALLBACK_DATA;

VOID EnumCallbacks(BYTE* buffer, DWORD size) {
    DWORD count = size;
    ULONG activeCount = 0;
	ULONG removedCount = 0;

    printf("\n[+] Process Callbacks:\n");

    while (count >= ENTRY_SIZE)
    {
        ULONG64 index = *(ULONG64*)buffer;
		buffer += 8;
		ULONG64 addr = *(ULONG64*)buffer;
		buffer += 8;
		PCHAR ModuleName = (PCHAR)buffer;
		buffer += 32;
		ULONG64 ModuleBase = *(ULONG64*)buffer;
		buffer += 8;

		count -= ENTRY_SIZE;

		if (addr != 0) {
			printf("    [%02llu] 0x%016llx  (%s + 0x%llx)\n", index, addr, ModuleName, ModuleBase);
			activeCount++;
		}
		if (addr == 0) {
			printf("    [%02llu] Removed callback %s\n", index, ModuleName);
			removedCount++;
		}
    }
}

HANDLE OpenDriver()
{
	HANDLE hDevice = CreateFileW(
		L"\\\\.\\Rm_ProcCallback",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Failed to open device: %d\n", GetLastError());
		return NULL;
	}
	return hDevice;
}

BOOL RemoveCallbacks(HANDLE hDevice, ULONG index)
{
	PROC_CALLBACK_DATA pProcCallbackData = { 0 };
	pProcCallbackData.Index = index;

	DWORD bytesReturned = 0;
	printf("[+] Removing process callback at index %lu...\n", index);

	if (!DeviceIoControl(
		hDevice,
		IOCTL_RM_PROC_CALLBACK,
		&pProcCallbackData, sizeof(pProcCallbackData),
		NULL, 0,
		&bytesReturned,
		NULL
	)) {
		printf("Failed to remove callback: %d\n", GetLastError());
		return FALSE;
	}

	printf("[+] Removed callback at index %lu.\n", index);

	return TRUE;
}


BOOL ListCallbacks(HANDLE hDevice)
{
	BYTE Buffer[4096] = { 0 };
	DWORD bytesReturned = 0;

	if (!DeviceIoControl(
		hDevice,
		IOCTL_READ_PROC_CALLBACK,
		NULL, 0,
		Buffer, sizeof(Buffer),
		&bytesReturned,
		NULL
	)) {
		printf("Failed to read from device: %d\n", GetLastError());
		return FALSE;
	}

	if (bytesReturned == 0) {
		printf("No process callbacks found.\n");
		return FALSE;
	}
	
	EnumCallbacks(Buffer, bytesReturned);
	return TRUE;
}

int main(int argc, char* argv[])
{
	HANDLE hDevice = OpenDriver();
	if (hDevice == INVALID_HANDLE_VALUE) {
		return 1;
	}

	if (argc >= 2) {
		ULONG index = strtoul(argv[1], NULL, 10);

		if (index >= MAX_ENTRIES) {
			printf("Index must be between 0 and %d\n", MAX_ENTRIES - 1);
			return 1;
		}

		if (!RemoveCallbacks(hDevice, index)) {
			return 1;
		}
	}

	if (!ListCallbacks(hDevice)) {
		return 1;
	}


    
    CloseHandle(hDevice);
    return 0;
}