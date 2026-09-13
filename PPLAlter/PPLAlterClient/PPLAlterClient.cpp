#include <windows.h>
#include <stdio.h>

#define IOCTL_SET_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _PPL_INFO {
	DWORD ProcessId;
	BYTE ProtectionLevel;
} PPL_INFO, * PPPL_INFO;	

int main(int argc, char* argv[]) {

	/*
	PS_PROTECTED_SYSTEM = 0x72
	PS_PROTECTED_WINTCB = 0x62
	PS_PROTECTED_WINDOWS = 0x52
	PS_PROTECTED_AUTHENTICODE = 0x12
	PS_PROTECTED_WINTCB_LIGHT = 0x61
	PS_PROTECTED_WINDOWS_LIGHT = 0x51
	PS_PROTECTED_LSA_LIGHT = 0x41
	PS_PROTECTED_ANTIMALWARE_LIGHT = 0x31
	PS_PROTECTED_AUTHENTICODE_LIGHT = 0x11
	*/
	if (argc < 2) {
		printf("Usage: %s <pid> <protection_level>\n", argv[0]);
		printf("Protection levels:\n");
		printf("  72 - PS_PROTECTED_SYSTEM\n");
		printf("  62 - PS_PROTECTED_WINTCB\n");
		printf("  52 - PS_PROTECTED_WINDOWS\n");
		printf("  12 - PS_PROTECTED_AUTHENTICODE\n");
		printf("  61 - PS_PROTECTED_WINTCB_LIGHT\n");
		printf("  51 - PS_PROTECTED_WINDOWS_LIGHT\n");
		printf("  41 - PS_PROTECTED_LSA_LIGHT\n");
		printf("  31 - PS_PROTECTED_ANTIMALWARE_LIGHT\n");
		printf("  11 - PS_PROTECTED_AUTHENTICODE_LIGHT\n");
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