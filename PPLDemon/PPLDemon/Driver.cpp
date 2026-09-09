#include <ntddk.h>
#include <ntstatus.h>

#define DRIVER_NAME "PPLDemon"
#define IOCTL_RM_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ADD_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\PPLDemon");
	UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\PPLDemon");
	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject = NULL;


	status = IoCreateDevice(
		DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
	);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("[%s]: Failed to create device: %08x\n", DRIVER_NAME, status);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;

	status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("[%s]: Failed to create symbolic link: %08x\n", DRIVER_NAME, status);
		IoDeleteDevice(DeviceObject);
		return status;
	}
	
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
	UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\PPLDemon");
	IoDeleteSymbolicLink(&symbolicLinkName);
	if (DeviceObject != NULL)
	{
		IoDeleteDevice(DeviceObject);
	}
	DbgPrint("[%s]: Driver unloaded\n", DRIVER_NAME);
}

NTSTATUS DriverIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	NTSTATUS status = STATUS_SUCCESS;


}