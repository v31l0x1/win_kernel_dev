#include <ntddk.h>
#include <ntstatus.h>

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

typedef struct _TERMINATE_PROCESS_REQUEST
{
	PCWSTR processName;
} TERMINATE_PROCESS_REQUEST, * PTERMINATE_PROCESS_REQUEST;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"\\Driver\\ProcTerminate_2");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\DosDevices\\ProcTerminate_2");
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS ntStatus = STATUS_SUCCESS;

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

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;

	ntStatus = IoCreateSymbolicLink(&SymbolicLinkName, &DriverName);
	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("[-] Failed to create symbolic link: 0x%X\n", ntStatus);
		IoDeleteDevice(DeviceObject);
		return ntStatus;
	}

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
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\DosDevices\\ProcTerminate_2");
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
	UNREFERENCED_PARAMETER(DeviceObject);

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_PROC_TERM)
	{

	}


}