#include <windows.h>
#include <stdio.h>


#define IOCTL_CRASHIT CTL_CODE(0x8000, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _CRASHIT_INPUT
{
	ULONG Pid;
} CRASHIT_INPUT, * PCRASHIT_INPUT;

int main(int argc, char* argv[])
{

}