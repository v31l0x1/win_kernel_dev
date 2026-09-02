#include <ntddk.h>
#include <ntstatus.h>

#define IOCTL_TERM_3 CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

typedef struct _PROC_TERM {
	ULONG Pid;
} PROC_TERM, *PPROC_TERM;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
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

	DbgPrint("[+] Driver loaded successfully\n");
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

			if (NT_SUCCESS(status))
			{
				// Need to resolve PsTerminateProcess or PspTerminateProcess dynamically.
				status = PspTerminateProces(hProcess, 0);
				ZwClose(hProcess);
			}
			else {
				DbgPrint("[-] Failed to open process with PID: %lu", ProcTerm->Pid);
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