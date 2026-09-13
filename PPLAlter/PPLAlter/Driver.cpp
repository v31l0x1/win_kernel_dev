#include <ntifs.h>
#include <ntstatus.h>
#include <minwindef.h>

#define DRIVER_NAME "PPLAlter"
#define IOCTL_SET_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

ULONG ProtectionOffset = 0;
typedef struct _PPL_INFO {
	ULONG ProcessId;
	ULONG ProtectionLevel;
} PPL_INFO, * PPPL_INFO;

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


NTSTATUS DriverDeviceIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SET_PPL) {

		if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(PPL_INFO)) {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else {
			PPPL_INFO PPLInfo = (PPPL_INFO)Irp->AssociatedIrp.SystemBuffer;
			ULONG Pid = PPLInfo->ProcessId;
			BYTE ProtectionLevel = (BYTE)PPLInfo->ProtectionLevel;

			PEPROCESS Process;
			status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)Pid, &Process);
			if (NT_SUCCESS(status)) {
				DbgPrint("[%s]: Found EPROCESS for PID %lu at %p\n", DRIVER_NAME, Pid, Process);

				ULONG_PTR Protection = (ULONG_PTR)Process + ProtectionOffset;
				*(BYTE*)Protection = ProtectionLevel;

				DbgPrint("[%s]: Set protection level to %u for PID %lu\n", DRIVER_NAME, ProtectionLevel, Pid);	
				ObDereferenceObject(Process);
				status = STATUS_SUCCESS;
			}
			else {
				status = STATUS_NOT_FOUND;
			}
		}
	}
	else {
		status = STATUS_INVALID_DEVICE_REQUEST;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
