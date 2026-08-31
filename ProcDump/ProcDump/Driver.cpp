#include <ntddk.h> 
#include <ntstatus.h>

#define IOCTL_DUMP_PROCESS CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);


typedef struct _DUMP_PROCESS_INPUT
{
	ULONG ProcessId;
	HANDLE ProcessHandle;
} DUMP_PROCESS_INPUT, * PDUMP_PROCESS_INPUT;


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"\\Driver\\ProcDump");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\DosDevices\\ProcDump");
	NTSTATUS ntStatus;
	PDEVICE_OBJECT DeviceObject = NULL;

	ntStatus = IoCreateDevice(
		DriverObject,
		0,
		&DriverName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
	);

	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("[-] Failed to create device object: 0x%X\n", ntStatus);
		return ntStatus;
	}

	ntStatus = IoCreateSymbolicLink(&SymbolicLinkName, &DriverName);

	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("[-] Failed to create symbolic link: 0x%X\n", ntStatus);
		IoDeleteDevice(DeviceObject);
		return ntStatus;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;

	DbgPrint("[+] Driver loaded successfully.\n");

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
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\DosDevices\\ProcDump");
	IoDeleteSymbolicLink(&SymbolicLinkName);

	if (DeviceObject != NULL)
	{
		IoDeleteDevice(DeviceObject);
	}

	DbgPrint("[+] Driver unloaded successfully.\n");

}


NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	ULONG len = 0;
	UNREFERENCED_PARAMETER(DeviceObject);

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_DUMP_PROCESS)
	{
		PDUMP_PROCESS_INPUT inputBuffer = (PDUMP_PROCESS_INPUT)Irp->AssociatedIrp.SystemBuffer;

		if (inputBuffer != NULL)
		{
			HANDLE hProcess = NULL;
			CLIENT_ID clientId;
			OBJECT_ATTRIBUTES objAttr;
			clientId.UniqueProcess = ULongToHandle(inputBuffer->ProcessId);
			clientId.UniqueThread = NULL;
			InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);
			
			status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);

			if (!NT_SUCCESS(status))
			{
				DbgPrint("[-] Failed to open process: 0x%X\n", status);
			}
			else {
				inputBuffer->ProcessHandle = hProcess;
				len = sizeof(DUMP_PROCESS_INPUT);
				DbgPrint("[+] Process handle obtained: 0x%p\n", hProcess);
				status = STATUS_SUCCESS;
			}
		}
		else {
			DbgPrint("[-] Input buffer is NULL\n");
			status = STATUS_INVALID_PARAMETER;
		}
	}
	else {
		DbgPrint("[-] Invalid IOCTL code: 0x%X\n", irpSp->Parameters.DeviceIoControl.IoControlCode);
		status = STATUS_INVALID_DEVICE_REQUEST;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}