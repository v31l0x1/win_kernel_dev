#include <ntddk.h>
#include <ntstatus.h>

#define IOCTL_OPEN_PROC_HANDLE	CTL_CODE(0x8000, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef unsigned long DWORD;

typedef struct {
	DWORD pid;
	HANDLE hProcess;
} OPEN_PROC_HANDLE_DATA;

NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DeviceCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID UnloadDriver(PDRIVER_OBJECT DriverObject);


extern "C" {
	NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {

		NTSTATUS ntStatus;
		UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\OpenHandle");
		UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\OpenHandle");
		PDEVICE_OBJECT deviceObject = NULL;

		ntStatus = IoCreateDevice(
			DriverObject,
			0,
			&DeviceName,
			FILE_DEVICE_UNKNOWN,
			FILE_DEVICE_SECURE_OPEN,
			FALSE,
			&deviceObject);

		if (!NT_SUCCESS(ntStatus)) {
			DbgPrint("[-] Couldn't create device object (0x%X)\n", ntStatus);
			return ntStatus;
		}

		DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreateClose;
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceCreateClose;
		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
		DriverObject->DriverUnload = UnloadDriver;

		ntStatus = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);

		if (!NT_SUCCESS(ntStatus)) {
			DbgPrint("[-] Couldn't create symbolic link (0x%X)\n", ntStatus);
			IoDeleteDevice(deviceObject);
			return ntStatus;
		}

		return STATUS_SUCCESS;
	}
}

NTSTATUS DeviceCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID UnloadDriver(PDRIVER_OBJECT DriverObject) {
	PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\OpenHandle");
	IoDeleteSymbolicLink(&SymbolicLink);
	if (deviceObject != NULL) {
		IoDeleteDevice(deviceObject);
	}
	DbgPrint("[+] Driver unloaded successfully\n");
}

NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
	auto len = 0;
	

	UNREFERENCED_PARAMETER(DeviceObject);

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_OPEN_PROC_HANDLE) {
		OPEN_PROC_HANDLE_DATA* data = (OPEN_PROC_HANDLE_DATA*)Irp->AssociatedIrp.SystemBuffer;

		DbgPrint("[+] IOCTL_OPEN_PROC_HANDLE received for PID: %lu\n", data->pid);

		if (data->pid == 0 || data->pid == 4) {
			DbgPrint("[-] Invalid PID: %lu\n", data->pid);
			ntStatus = STATUS_INVALID_PARAMETER;
		}
		else {
			HANDLE hProcess = NULL;
			CLIENT_ID clientId;
			OBJECT_ATTRIBUTES objAttr;

			clientId.UniqueProcess = UlongToHandle(data->pid);
			clientId.UniqueThread = NULL;
			InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);

			ntStatus = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);
			if (!NT_SUCCESS(ntStatus)) {
				DbgPrint("[-] ZwOpenProcess failed (0x%X)\n", ntStatus);
			}
			else {
				data->hProcess = hProcess;
				len = sizeof(OPEN_PROC_HANDLE_DATA);
				DbgPrint("[+] Process handle opened successfully: 0x%p\n", hProcess);
				ntStatus = STATUS_SUCCESS;
			}
		}
	}

	Irp->IoStatus.Status = ntStatus;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);


	return ntStatus;
}