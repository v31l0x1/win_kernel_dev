#include <Windows.h>
#include <stdio.h>
#include <winioctl.h>
#include "defines.h"

#define IOCTL_RM_PROC_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

VOID EnumCallbacks(BYTE* buffer, DWORD size) {
    DWORD count = size;
    int index_comp = 0;

    printf("\n[+] Process Callbacks:\n");
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
            printf("    [%02llu] 0x%llx", index, addr);
            printf(" (%s + 0x%llx)\n", ModuleName, ModuleBase);
        }

        count -= 40;
        index_comp++;
    }
}

int main(int argc, char* argv[])
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
        return 1;
    }

    BYTE Buffer[4096] = { 0 };
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(
        hDevice,
        IOCTL_RM_PROC_CALLBACK,
        NULL, 0,
        Buffer, sizeof(Buffer),
        &bytesReturned,
        NULL
    )) {
        printf("Failed to read from device: %d\n", GetLastError());
        CloseHandle(hDevice);
        return 1;
    }

    if (bytesReturned != 0) {
        EnumCallbacks(Buffer, bytesReturned);
    }

    CloseHandle(hDevice);
    return 0;
}