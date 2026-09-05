#include <ntddk.h>
#include <ntstatus.h>

#define DRIVER_NAME "Rm_ProcCallback"
#define IOCTL_RM_PROC_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _PROC_CALLBACK_DATA {
	ULONG ProcessId;
} PROC_CALLBACK_DATA, * PPROC_CALLBACK_DATA;

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DriverDeviceControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\Rm_ProcCallback");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\Rm_ProcCallback");
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	status = IoCreateDevice(
		DriverObject,
		0,
		&DeviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject);

	if (!NT_SUCCESS(status)) {
		DbgPrint("[%s]: Failed to create device: 0x%X\n", DRIVER_NAME, status);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
	DriverObject->DriverUnload = DriverUnload;

	status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
	
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to create symbolic link: 0x%X\n", status);
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
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\Rm_ProcCallback");

	IoDeleteSymbolicLink(&SymbolicLinkName);

	if (DeviceObject != NULL) {
		IoDeleteDevice(DeviceObject);
	}
	
	DbgPrint("[%s]: Driver unloaded successfully\n", DRIVER_NAME);
}

NTSTATUS DriverDeviceControl(PDEVICE_OBJECT, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_RM_PROC_CALLBACK && irpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(PROC_CALLBACK_DATA))
	{
		PPROC_CALLBACK_DATA ProcCallbackData = (PPROC_CALLBACK_DATA)Irp->AssociatedIrp.SystemBuffer;

		if (ProcCallbackData != NULL)
		{
			DbgPrint("[%s]: Received process ID: %lu\n", DRIVER_NAME, ProcCallbackData->ProcessId);
			status = STATUS_SUCCESS;
		}
		else {
			DbgPrint("[%s]: Invalid input buffer\n", DRIVER_NAME);
			status = STATUS_INVALID_PARAMETER;
		}
	}
	else {
		DbgPrint("[%s]: Invalid IOCTL code or input buffer length\n", DRIVER_NAME);
		status = STATUS_INVALID_PARAMETER;
	}

	Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_INVALID_PARAMETER;
}