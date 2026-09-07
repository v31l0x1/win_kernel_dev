#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winioctl.h>

#define IOCTL_READ_OBJ_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RM_OBJ_CALLBACK   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define OB_TYPE_PROCESS 0
#define OB_TYPE_THREAD  1

#define ENTRY_SIZE 280
#define MAX_ENTRIES 64

typedef struct _OBJ_CALLBACK_DATA {
    int Index;
    int Type;
} OBJ_CALLBACK_DATA, * POBJ_CALLBACK_DATA;

typedef struct _OBJ_CALLBACK_INFO {
    ULONG64 PreOperation;
    ULONG64 PostOperation;
    ULONG64 ModuleBase;
    CHAR    ModuleName[256];
} OBJ_CALLBACK_INFO, * POBJ_CALLBACK_INFO;

const char* TypeToStr(int type)
{
    return (type == OB_TYPE_PROCESS) ? "Process" : "Thread";
}

VOID EnumCallbacks(BYTE* buffer, DWORD size)
{
    DWORD count = size;
    ULONG activeCount = 0;
    ULONG64 maxIndex = 0;
    BOOL hasActive = FALSE;

    BYTE* tempBuffer = buffer;
    DWORD tempCount = size;
    ULONG64 idx = 0;
    while (tempCount >= ENTRY_SIZE) {
        POBJ_CALLBACK_INFO info = (POBJ_CALLBACK_INFO)tempBuffer;
        tempBuffer += ENTRY_SIZE;
        tempCount -= ENTRY_SIZE;

        if (info->PreOperation != 0 || info->PostOperation != 0) {
            hasActive = TRUE;
            maxIndex = idx;
        }
        idx++;
    }

    printf("\n[+] Object Callbacks (Process type first, then Thread type):\n");
    printf("    -----------------------------------------------------------------------------\n");
    printf("    Idx  Type     PreOp / PostOp                     Owner\n");
    printf("    -----------------------------------------------------------------------------\n");

    if (!hasActive) {
        printf("    No active callbacks found.\n");
        printf("    -----------------------------------------------------------------------------\n");
        return;
    }

    ULONG64 index = 0;
    while (count >= ENTRY_SIZE && index <= maxIndex)
    {
        POBJ_CALLBACK_INFO info = (POBJ_CALLBACK_INFO)buffer;
        buffer += ENTRY_SIZE;
        count -= ENTRY_SIZE;

        int type = (index < MAX_ENTRIES) ? OB_TYPE_PROCESS : OB_TYPE_THREAD;
        ULONG64 typedIndex = (type == OB_TYPE_PROCESS) ? index : index - MAX_ENTRIES;

        if (info->PreOperation != 0 || info->PostOperation != 0) {
            printf("    [%02llu] %-8s pre=0x%016llx post=0x%016llx  (%s + 0x%llx)\n",
                typedIndex, TypeToStr(type),
                info->PreOperation, info->PostOperation,
                info->ModuleName,
                info->PreOperation ? (info->PreOperation - info->ModuleBase) : (info->PostOperation - info->ModuleBase));
            activeCount++;
        }
        else {
            if (info->ModuleName[0] != '\0') {
                printf("    [%02llu] %-8s removed callback (%s)\n", typedIndex, TypeToStr(type), info->ModuleName);
            }
            else {
                printf("    [%02llu] %-8s removed callback\n", typedIndex, TypeToStr(type));
            }
        }
        index++;
    }

    printf("    -----------------------------------------------------------------------------\n");
}

HANDLE GetDriverHandle(VOID)
{
    HANDLE hDevice = CreateFileW(
        L"\\\\.\\Rm_ObjCallback",
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

BOOL RemoveCallback(HANDLE hDevice, ULONG index, int type)
{
    OBJ_CALLBACK_DATA data = { 0 };
    data.Index = (int)index;
    data.Type = type;

    DWORD bytesReturned = 0;
    printf("[+] Removing %s object callback at index %lu...\n", TypeToStr(type), index);

    if (!DeviceIoControl(
        hDevice,
        IOCTL_RM_OBJ_CALLBACK,
        &data, sizeof(data),
        NULL, 0,
        &bytesReturned,
        NULL
    )) {
        printf("Failed to remove callback: %d\n", GetLastError());
        return FALSE;
    }

    printf("[+] Removed %s callback at index %lu.\n", TypeToStr(type), index);
    return TRUE;
}

BOOL ListCallbacks(HANDLE hDevice)
{
    BYTE Buffer[128 * ENTRY_SIZE] = { 0 };
    DWORD bytesReturned = 0;

    if (!DeviceIoControl(
        hDevice,
        IOCTL_READ_OBJ_CALLBACK,
        NULL, 0,
        Buffer, sizeof(Buffer),
        &bytesReturned,
        NULL
    )) {
        printf("Failed to read from device: %d\n", GetLastError());
        return FALSE;
    }

    if (bytesReturned == 0) {
        printf("No object callbacks found.\n");
        return FALSE;
    }

    EnumCallbacks(Buffer, bytesReturned);
    return TRUE;
}

int main(int argc, char* argv[])
{
    HANDLE hDevice = GetDriverHandle();
    if (hDevice == NULL) {
        return 1;
    }

    if (argc >= 3) {
        ULONG index = strtoul(argv[1], NULL, 10);
        int type = (int)strtol(argv[2], NULL, 10);

        if (type != OB_TYPE_PROCESS && type != OB_TYPE_THREAD) {
            printf("Type must be 0 (Process) or 1 (Thread)\n");
            CloseHandle(hDevice);
            return 1;
        }

        if (index >= MAX_ENTRIES) {
            printf("Index must be between 0 and %d\n", MAX_ENTRIES - 1);
            CloseHandle(hDevice);
            return 1;
        }

        if (!RemoveCallback(hDevice, index, type)) {
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