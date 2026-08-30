#include <ntddk.h>
#include <ntstatus.h>


NTSTATUS CreateCloseFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID UnloadDriver(PDRIVER_OBJECT DriverObject);

extern "C" {
	NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {

		NTSTATUS ntStatus;
		UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\TestDriver");
		UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\TestDriver");
		PDEVICE_OBJECT deviceObject = NULL;


		ntStatus = IoCreateDevice(
			DriverObject,
			0,
			&DeviceName,
			FILE_DEVICE_UNKNOWN,
			FILE_DEVICE_SECURE_OPEN,
			FALSE,
			&deviceObject
		);

		if (!NT_SUCCESS(ntStatus)) {
			DbgPrint("Failed to create device object: 0x%X\n", ntStatus);
			return ntStatus;
		}

		DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseFunction;
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseFunction;
		DriverObject->DriverUnload = UnloadDriver;


		ntStatus = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);

		if (!NT_SUCCESS(ntStatus)) {
			DbgPrint("Failed to create symbolic link: 0x%X\n", ntStatus);
			IoDeleteDevice(deviceObject);
			return ntStatus;
		}

		DbgPrint("Driver loaded successfully.\n");

		DbgPrint("Hello from the kernel mode driver!\n");

		return STATUS_SUCCESS;
	}
}

NTSTATUS CreateCloseFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID UnloadDriver(PDRIVER_OBJECT DriverObject) {

	PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;

	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\TestDriver");
	IoDeleteSymbolicLink(&SymbolicLink);

	if (deviceObject != NULL) {
		IoDeleteDevice(deviceObject);
	}

	DbgPrint("Driver unloaded successfully.\n");
}