#include <ntddk.h>
#include <ntstatus.h>

#define DRIVER_NAME "PPLAlter"

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DriverDeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"\\Device\\PPLAlter");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\PPLAlter");
	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject = NULL;

	status = IoCreateDevice(
		DriverObject,
		0,
		&DriverName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
	);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("[%s]: Failed to create device object (0x%X)\n", DRIVER_NAME, status);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;

	DriverObject->Flags |= DO_BUFFERED_IO;

	status = IoCreateSymbolicLink(&SymbolicLinkName, &DriverName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[%s]: Failed to create symbolic link (0x%X)\n", DRIVER_NAME, status);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	DbgPrint("[%s]: Driver loaded successfully\n", DRIVER_NAME);

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
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\PPLAlter");
	IoDeleteSymbolicLink(&SymbolicLinkName);
	if (DeviceObject != NULL) {
		IoDeleteDevice(DeviceObject);
	}
	DbgPrint("[%s]: Driver unloaded successfully\n", DRIVER_NAME);
}
