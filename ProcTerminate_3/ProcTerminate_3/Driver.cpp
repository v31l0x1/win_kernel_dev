#include <ntddk.h>
#include <ntstatus.h>

#pragma warning(disable : 4996)

#define IOCTL_TERM_3 CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

typedef NTSTATUS(NTAPI* ZwQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(*PSP_TERMINATE_PROCESS)(PEPROCESS Process, NTSTATUS ExitStatus);

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, * PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;

PSP_TERMINATE_PROCESS g_PspTerminateProcess = NULL;

typedef struct _PROC_TERM {
	ULONG Pid;
} PROC_TERM, *PPROC_TERM;

PVOID FindPattern(PUCHAR baseAddress, SIZE_T size, PUCHAR pattern, SIZE_T patternSize)
{
	for (size_t i = 0; i <= size - patternSize; i++) 
	{
		BOOLEAN found = TRUE;
		for (size_t j = 0; j < patternSize; j++) {
			if (baseAddress[i + j] != pattern[j]) {
				found = FALSE;
				break;
			}
		}
		if (found) {
			return (PVOID)(baseAddress + i);
		}
	}
	return NULL;
}

NTSTATUS GetNtosKrnlBaseAndSize(PVOID* BaseAddress, PSIZE_T Size)
{
	UNICODE_STRING routineName;
	RtlInitUnicodeString(&routineName, L"ZwQuerySystemInformation");
	ZwQuerySystemInformation_t pZwQuerySystemInformation =
		(ZwQuerySystemInformation_t)MmGetSystemRoutineAddress(&routineName);

	if (!pZwQuerySystemInformation) {
		return STATUS_NOT_FOUND;
	}

	ULONG returnLength = 0;
	/* https://ntdoc.m417z.com/system_information_class */
	NTSTATUS status = pZwQuerySystemInformation(11, NULL, 0, &returnLength); // SystemModuleInformation
	if (status != STATUS_INFO_LENGTH_MISMATCH) {
		return status;
	}

	PVOID buffer = ExAllocatePoolWithTag(NonPagedPool, returnLength, 'tagK');
	if (!buffer) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = pZwQuerySystemInformation(11, buffer, returnLength, &returnLength); // SystemModuleInformation
	if (!NT_SUCCESS(status)) {
		ExFreePoolWithTag(buffer, 'tagK');
		return status;
	}

	PRTL_PROCESS_MODULES modules = (PRTL_PROCESS_MODULES)buffer;
	for (ULONG i = 0; i < modules->NumberOfModules; i++) {
		PCSTR fileName = (PCSTR)(modules->Modules[i].FullPathName + modules->Modules[i].OffsetToFileName);
		if (_stricmp(fileName, "ntoskrnl.exe") == 0) {
			*BaseAddress = modules->Modules[i].ImageBase;
			*Size = modules->Modules[i].ImageSize;
			ExFreePoolWithTag(buffer, 'tagK');
			return STATUS_SUCCESS;
		}
	}

	ExFreePoolWithTag(buffer, 'tagK');
	return STATUS_NOT_FOUND;
}

NTSTATUS ResolvePspTerminateProcess(VOID)
{
	PVOID ntoskrnlBase = NULL;
	SIZE_T ntoskrnlSize = 0;

	NTSTATUS status = GetNtosKrnlBaseAndSize(&ntoskrnlBase, &ntoskrnlSize);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[-] Failed to get ntoskrnl base: %08X\n", status);
		return status;
	}

	DbgPrint("[+] ntoskrnl.exe base: %p size: 0x%llx\n", ntoskrnlBase, (ULONGLONG)ntoskrnlSize);

	//UCHAR pattern[] = {
	//	0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20,
	//	0x65, 0x48, 0x8B, 0x3C, 0x25, 0x88, 0x01, 0x00, 0x00,
	//	0x66, 0xFF, 0x8F, 0xE4, 0x01, 0x00, 0x00
	//};

	UCHAR pattern[] = {
		0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20,
		0x65, 0x48, 0x8B, 0x3C, 0x25, 0x88, 0x01, 0x00, 0x00,
		0x44, 0x8B, 0xC2, 0x41, 0xB9, 0x01, 0x00, 0x00, 0x00
	};

	PVOID address = FindPattern((PUCHAR)ntoskrnlBase, ntoskrnlSize, pattern, sizeof(pattern));
	if (!address) {
		DbgPrint("[-] Pattern for PspTerminateProcess not found\n");
		return STATUS_NOT_FOUND;
	}

	g_PspTerminateProcess = (PSP_TERMINATE_PROCESS)address;
	DbgPrint("[+] PspTerminateProcess resolved at: %p\n", address);
	return STATUS_SUCCESS;
}


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\ProcTerminate_3");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\ProcTerminate_3");
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	status = IoCreateDevice(
		DriverObject,
		0,
		&DeviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
	);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("[-] Failed to create device: %08X\n", status);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;


	status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[-] Failed to create symbolic link: %08X\n", status);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	status = ResolvePspTerminateProcess();
	if (!NT_SUCCESS(status)) {
		DbgPrint("[-] Failed to resolve PspTerminateProcess: %08X\n", status);
		IoDeleteSymbolicLink(&SymbolicLinkName);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	DbgPrint("[+] Driver loaded successfully\n");
	return STATUS_SUCCESS;
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}	

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\ProcTerminate_3");
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	IoDeleteSymbolicLink(&SymbolicLinkName);
	if (DeviceObject != NULL) {
		IoDeleteDevice(DriverObject->DeviceObject);
	}
	DbgPrint("[+] Driver unloaded successfully\n");
}


NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	
	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_TERM_3 && irpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(ULONG)) {
		
		PPROC_TERM ProcTerm = (PPROC_TERM)Irp->AssociatedIrp.SystemBuffer;

		if (ProcTerm != NULL)
		{
			HANDLE hProcess = NULL;
			CLIENT_ID clientId;
			OBJECT_ATTRIBUTES objectAttributes;
			clientId.UniqueProcess = UlongToHandle(ProcTerm->Pid);
			clientId.UniqueThread = NULL;
			InitializeObjectAttributes(&objectAttributes, NULL, 0, NULL, NULL);

			status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objectAttributes, &clientId);

			if (NT_SUCCESS(status)) {
				PEPROCESS processObject = NULL;
				status = ObReferenceObjectByHandle(
					hProcess, 0, NULL, KernelMode,
					(PVOID*)&processObject, NULL
				);

				if (NT_SUCCESS(status)) {
					status = g_PspTerminateProcess(processObject, 0);
					ObDereferenceObject(processObject);
				}
				ZwClose(hProcess);
			}
			else {
				DbgPrint("[-] Failed to open process with PID: %lu\n", ProcTerm->Pid);
			}

			status = STATUS_SUCCESS;
		}
		else {
			DbgPrint("[-] Invalid input buffer");
			status = STATUS_INVALID_PARAMETER;
		}
	}
	else {
		DbgPrint("[-] Invalid IOCTL code");
		status = STATUS_INVALID_DEVICE_REQUEST;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}