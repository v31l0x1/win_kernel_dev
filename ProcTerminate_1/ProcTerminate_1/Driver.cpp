#include <ntddk.h>
#include <ntstatus.h>

#define IOCTL_TERMINATE_PROC CTL_CODE( 0x8001, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )
#define PROCESS_TERMINATE 0x0001

typedef unsigned long DWORD;

typedef struct {
	DWORD Pid;
} ProcTerm;

NTSTATUS DeviceCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS GetPsTerminateProcessAddress();
VOID UnloadDriver(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\ProcTerminate");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\ProcTerminate");
	NTSTATUS ntStatus;
	PDEVICE_OBJECT DeviceObject = NULL;

	ntStatus = IoCreateDevice(
		DriverObject,
		0,
		&DeviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
	);

	if (!NT_SUCCESS(ntStatus)) {
		DbgPrint("[-] Failed to create device");
		return ntStatus;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = UnloadDriver;

	NTSTATUS status = GetPsTerminateProcessAddress();
	if (!NT_SUCCESS(status)) {
		DbgPrint("[-] Failed to resolve PsTerminateProcess");
		return status;
	}

	ntStatus = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);

	if (!NT_SUCCESS(ntStatus)) {
		DbgPrint("[-] Failed to create symbolic link");
		IoDeleteDevice(DeviceObject);
		return ntStatus;
	}

	return STATUS_SUCCESS;
}


NTSTATUS DeviceCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID UnloadDriver(PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\ProcTerminate");

	IoDeleteSymbolicLink(&SymbolicLinkName);
	if (DeviceObject != NULL) {
		IoDeleteDevice(DeviceObject);
	}

	DbgPrint("[+] Driver unloaded successfully");
}


NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS ntStatus = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(DeviceObject);

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_TERMINATE_PROC) {

		ProcTerm* procTerm = (ProcTerm*)Irp->AssociatedIrp.SystemBuffer;

		if (procTerm != NULL) {
			HANDLE procHandle;
			OBJECT_ATTRIBUTES objAttr;
			CLIENT_ID clientId;
			clientId.UniqueProcess = UlongToHandle(procTerm->Pid);
			clientId.UniqueThread = NULL;
			InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);

			ntStatus = ZwOpenProcess(&procHandle, PROCESS_TERMINATE, &objAttr, &clientId);

			if (NT_SUCCESS(ntStatus)) {
				ntStatus = ZwTerminateProcess(procHandle, 0);
				ZwClose(procHandle);
			}
			else {
				DbgPrint("[-] Failed to open process with PID: %lu", procTerm->Pid);
			}	

			ntStatus = STATUS_SUCCESS;
		}
		else {
			DbgPrint("[-] Invalid input buffer");
			ntStatus = STATUS_INVALID_PARAMETER;
		}
	}
	else {
		DbgPrint("[-] Invalid IOCTL code");
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
	}

	Irp->IoStatus.Status = ntStatus;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return ntStatus;
}