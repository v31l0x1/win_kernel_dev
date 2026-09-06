#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winioctl.h>

#define IOCTL_READ_REG_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RM_REG_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define ENTRY_SIZE 272
#define MAX_ENTRIES 64

typedef struct _REG_CALLBACK_DATA {
    ULONG Index;
} REG_CALLBACK_DATA, * PREG_CALLBACK_DATA;

VOID EnumCallbacks(BYTE* buffer, DWORD size) {
    DWORD count = size;
    ULONG activeCount = 0;
    ULONG64 maxIndex = 0;
    BOOL hasActive = FALSE;

    BYTE* tempBuffer = buffer;
    DWORD tempCount = size;
    ULONG64 idx = 0;
    while (tempCount >= ENTRY_SIZE) {
        ULONG64 addr = *(ULONG64*)tempBuffer;
        tempBuffer += 8;
        tempBuffer += 256;
        tempBuffer += 8;
        tempCount -= ENTRY_SIZE;

        if (addr != 0) {
            hasActive = TRUE;
            maxIndex = idx;
        }
        idx++;
    }

    printf("\n[+] Registry Callbacks:\n");
    printf("    ---------------------------------------------------------------------\n");

    if (!hasActive) {
        printf("    No active callbacks found.\n");
        return;
    }

    ULONG64 index = 0;
    while (count >= ENTRY_SIZE && index <= maxIndex)
    {
        ULONG64 addr = *(ULONG64*)buffer;
        buffer += 8;

        CHAR* ModuleName = (CHAR*)buffer;
        buffer += 256;

        ULONG64 ModuleBase = *(ULONG64*)buffer;
        buffer += 8;

        count -= ENTRY_SIZE;

        if (addr != 0) {
            printf("    [%02llu] 0x%016llx  (%s + 0x%llx)\n", index, addr, ModuleName, ModuleBase);
            activeCount++;
        }
        else {
            if (index == 0) {
                index++;
                continue;
            }
            if (ModuleName[0] != '\0') {
                printf("    [%02llu] Removed callback (%s)\n", index, ModuleName);
            }
            else {
                printf("    [%02llu] Removed callback\n", index);
            }
        }
        index++;
    }

    printf("    ---------------------------------------------------------------------\n");
}

HANDLE GetDriverHandle(VOID)
{
    HANDLE hDevice = CreateFileW(
        L"\\\\.\\Rm_RegistryCallback",
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
    REG_CALLBACK_DATA pRegCallbackData = { 0 };
    pRegCallbackData.Index = index;

    DWORD bytesReturned = 0;
    printf("[+] Removing registry callback at index %lu...\n", index);

    if (!DeviceIoControl(
        hDevice,
        IOCTL_RM_REG_CALLBACK,
        &pRegCallbackData, sizeof(pRegCallbackData),
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
    BYTE Buffer[18000] = { 0 };
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(
        hDevice,
        IOCTL_READ_REG_CALLBACK,
        NULL, 0,
        Buffer, sizeof(Buffer),
        &bytesReturned,
        NULL
    )) {
        printf("Failed to read from device: %d\n", GetLastError());
        return FALSE;
    }

    if (bytesReturned == 0) {
        printf("No registry callbacks found.\n");
        return FALSE;
    }

    EnumCallbacks(Buffer, bytesReturned);
    return TRUE;
}

int main(int argc, char* argv[])
{
    HANDLE hDevice = GetDriverHandle();
    if (hDevice == INVALID_HANDLE_VALUE) {
        return 1;
    }

    if (argc >= 2) {
        ULONG index = strtoul(argv[1], NULL, 10);

        if (index >= MAX_ENTRIES) {
            printf("Index must be between 0 and %d\n", MAX_ENTRIES - 1);
            CloseHandle(hDevice);
            return 1;
        }

        if (!RemoveCallbacks(hDevice, index)) {
            CloseHandle(hDevice);
            return 1;
        }
    }

    if (!ListCallbacks(hDevice)) {
        CloseHandle(hDevice);
        return 1;
    }

    CloseHandle(hDevice);
    return 0;
}