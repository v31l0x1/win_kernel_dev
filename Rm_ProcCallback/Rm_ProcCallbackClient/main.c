#include <Windows.h>
#include <stdio.h>
#include "defines.h"

VOID EnumCallbacks(BYTE* buffer, DWORD size) {
    auto count = size;
    int index_comp = 0;

    printf("\n[*] Process Callbacks\n");
    while (count > 0)
    {
        auto index = *(ULONG64*)buffer;
        buffer += 8;
        auto addr = *(ULONG64*)buffer;
        buffer += 8;

        count -= 16;

        auto ModuleName = (CHAR*)buffer;
        buffer += 32;
        auto ModuleBase = *(ULONG64*)buffer;
        buffer += 8;

        if (addr != 0) {
            printf("[%02llu] 0x%llx", index, addr);
            printf(" (%s + 0x%llx)\n", ModuleName, ModuleBase);
        }
        else {
            printf("[%02llu] (Removed)\n", index);
        }


        count = count - 8 - 32;
        index_comp++;
    }
}

int main(int argc, char* argv[])
{
	//HANDLE hDevice = CreateFileW(
	//	L"\\\\.\\Rm_ProcCallback",
	//	GENERIC_READ | GENERIC_WRITE,
	//	FILE_SHARE_WRITE,
	//	NULL,
	//	OPEN_EXISTING,
	//	FILE_ATTRIBUTE_NORMAL,
	//	NULL
	//);

	//if (hDevice == INVALID_HANDLE_VALUE) {
	//	printf("Failed to open device: %d\n", GetLastError());
	//	return 1;
	//}

	HANDLE hDevice = CreateFileW(
		L"\\\\.\\Rm_ProcCallback",
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Failed to open device: %d\n", GetLastError());
		return 1;
	}

	BYTE Buffer[1024] = { 0 };
	BOOL success = 0;
	int count = 0;

	while (TRUE) {
		DWORD bytesReturned = 0;

		if (!ReadFile(hDevice, Buffer, sizeOf(Buffer), &bytesReturned, NULL)) {
			printf("Failed to read from device: %d\n", GetLastError());
			return 1;
		}

		if (bytesReturned != 0) {
			EnumCallbacks(Buffer, bytesReturned);
			success = 1;
			break;
		}
		
		if (count == 65) {
			success = 1;
			break;
		}
		
		count++;
	}

	CloseHandle(hDevice);

	return 0;
}