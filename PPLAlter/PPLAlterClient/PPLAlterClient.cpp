#include <windows.h>
#include <stdio.h>

#define IOCTL_SET_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _PPL_INFO {
	DWORD ProcessId;
	BYTE ProtectionLevel;
} PPL_INFO, * PPPL_INFO;	

int main(int argc, char* argv[]) {

	if (argc < 2) {
		printf("Usage: %s <pid> <protection_level>\n", argv[0]);
		return 1;
	}

	DWORD pid = atoi(argv[1]);
	BYTE protection = (BYTE)atoi(argv[2]);

	
	HANDLE hFile = CreateFileW(
		L"\\\\.\\PPLAlter",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile == INVALID_HANDLE_VALUE) {
		printf("Failed to open device: %d\n", GetLastError());
		return 1;
	}

	PPL_INFO pplInfo;
	pplInfo.ProcessId = pid;
	pplInfo.ProtectionLevel = protection;

	DWORD bytesReturned;
	BOOL Success = DeviceIoControl(
		hFile,
		IOCTL_SET_PPL,
		&pplInfo,
		sizeof(PPL_INFO),
		NULL,
		0,
		&bytesReturned,
		NULL
	);

	if (!Success) {
		printf("DeviceIoControl failed: %d\n", GetLastError());
		CloseHandle(hFile);
		return 1;
	}

	printf("Successfully set protection level %d for process %d\n", protection, pid);

	CloseHandle(hFile);
	return 0;
}