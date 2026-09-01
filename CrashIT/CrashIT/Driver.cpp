//#include <ntddk.h>
#include <ntifs.h>
#include <ntstatus.h>

#define IOCTL_CRASHIT CTL_CODE(0x8000, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

extern "C" NTSTATUS ZwQueryInformationProcess(
	HANDLE ProcessHandle,
	PROCESSINFOCLASS ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength,
	PULONG ReturnLength);

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

typedef struct _CRASHIT_INPUT
{
	ULONG Pid;
} CRASHIT_INPUT, * PCRASHIT_INPUT;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\CrashIT");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\CrashIT");
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
		DbgPrint("[-] Failed to create device: %08x\n", status);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;

	DriverObject->Flags |= DO_BUFFERED_IO;

	status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[-] Failed to create symbolic link: %08x\n", status);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	DbgPrint("[+] Driver loaded successfully\n");

	return STATUS_SUCCESS;
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\CrashIT");
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	IoDeleteSymbolicLink(&SymbolicLinkName);
	if (DeviceObject)
	{
		IoDeleteDevice(DeviceObject);
	}
	DbgPrint("[+] Driver unloaded successfully\n");
}

NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == 0x222000 && irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG))
	{
		PCRASHIT_INPUT crashInput = (PCRASHIT_INPUT)Irp->AssociatedIrp.SystemBuffer;

		if (crashInput != NULL)
		{
			PEPROCESS Process;
			HANDLE hProcess = UlongToHandle(crashInput->Pid);

			status = PsLookupProcessByProcessId(hProcess, &Process);
			if (NT_SUCCESS(status))
			{
				HANDLE hProcessHandle;
	
				status = ObOpenObjectByPointer(
					hProcess,
					OBJ_KERNEL_HANDLE,
					NULL,
					PROCESS_ALL_ACCESS,
					*PsProcessType, 
					KernelMode,
					&hProcessHandle);
				if (NT_SUCCESS(status)) {
					KAPC_STATE apcState;

					KeStackAttachProcess(Process, &apcState);
					
					PROCESS_BASIC_INFORMATION pbi;
					ULONG returnLength;

					status = ZwQueryInformationProcess(hProcessHandle, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength);

					if (NT_SUCCESS(status))
					{
						PVOID baseAddress = pbi.PebBaseAddress;
						SIZE_T size = 4096;

						if (baseAddress != NULL) {

							PMDL mdl = IoAllocateMdl(baseAddress, (ULONG)size, FALSE, FALSE, NULL);
							if (mdl != NULL) {
								__try {
									MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);

									PVOID mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority);
									if (mappedAddress != NULL) {

										RtlFillMemory(mappedAddress, size, 0xCC);

										MmUnmapLockedPages(mappedAddress, mdl);
									}

									MmUnlockPages(mdl);
								}
								__except (EXCEPTION_EXECUTE_HANDLER) {
									DbgPrint("[-] Exception occurred while accessing the PEB: %08x\n", GetExceptionCode());
								}

								IoFreeMdl(mdl);
							}
						}
					}
					else {
						DbgPrint("[-] Failed to query process information: %08x\n", status);
					}

					KeUnstackDetachProcess(&apcState);

					ZwClose(hProcessHandle);
				}

			}
			else {
				status = STATUS_NOT_FOUND;
				DbgPrint("[-] Failed to lookup process by PID %lu: %08x\n", crashInput->Pid, status);
			}
		}
		else {
			status = STATUS_INVALID_PARAMETER;
			DbgPrint("[-] Invalid input buffer\n");
		}
	}
	else {
		status = STATUS_INVALID_DEVICE_REQUEST;
		DbgPrint("[-] Invalid IOCTL code or input buffer length\n");
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IofCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
