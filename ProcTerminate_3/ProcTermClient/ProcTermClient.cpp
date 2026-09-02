#include <Windows.h>
#include <stdio.h>

#define IOCTL_TERM_3 CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _PROC_TERM {
	ULONG Pid;
} PROC_TERM, * PPROC_TERM;


int main(int argc, char* argv[]) {

}