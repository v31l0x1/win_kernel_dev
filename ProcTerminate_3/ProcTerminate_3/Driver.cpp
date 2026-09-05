#include <ntifs.h>
#include <ntstatus.h>

#define IOCTL_TERM_3 CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifndef PROCESS_TERMINATE
#define PROCESS_TERMINATE (0x0001)
#endif

typedef struct _PROC_TERM {
	ULONG Pid;
} PROC_TERM, * PPROC_TERM;

NTSTATUS DriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);


typedef PVOID(*PsGetProcessSectionBaseAddress)(PEPROCESS Process);
typedef NTSTATUS(*MmUnmapViewOfSection)(PEPROCESS Process, PVOID BaseAddress);

PsGetProcessSectionBaseAddress g_PsGetProcessSectionBaseAddress = NULL;
MmUnmapViewOfSection g_MmUnmapViewOfSection = NULL;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\ProcTerminate_3");
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\ProcTerminate_3");
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	status = IoCreateDevice(
		DriverObject,
		0,
		&DeviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&DeviceObject
	);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("[-] Failed to create device: %08X\n", status);
		return status;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
	DriverObject->DriverUnload = DriverUnload;

	status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("[-] Failed to create symbolic link: %08X\n", status);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	UNICODE_STRING funcName;
	RtlInitUnicodeString(&funcName, L"PsGetProcessSectionBaseAddress");
	g_PsGetProcessSectionBaseAddress = (PsGetProcessSectionBaseAddress)MmGetSystemRoutineAddress(&funcName);

	if (!g_PsGetProcessSectionBaseAddress) {
		DbgPrint("[-] Failed to get PsGetProcessSectionBaseAddress\n");
		IoDeleteSymbolicLink(&SymbolicLinkName);
		IoDeleteDevice(DeviceObject);
		return STATUS_UNSUCCESSFUL;
	}

	RtlInitUnicodeString(&funcName, L"MmUnmapViewOfSection");
	g_MmUnmapViewOfSection = (MmUnmapViewOfSection)MmGetSystemRoutineAddress(&funcName);

	if (!g_MmUnmapViewOfSection) {
		DbgPrint("[-] Failed to get MmUnmapViewOfSection\n");
		IoDeleteSymbolicLink(&SymbolicLinkName);
		IoDeleteDevice(DeviceObject);
		return STATUS_UNSUCCESSFUL;
	}

	DbgPrint("[+] Driver loaded successfully\n");
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
	UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\ProcTerminate_3");
	PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
	IoDeleteSymbolicLink(&SymbolicLinkName);
	if (DeviceObject != NULL) {
		IoDeleteDevice(DriverObject->DeviceObject);
	}
	DbgPrint("[+] Driver unloaded successfully\n");
}

NTSTATUS KillProcessByPid(_In_ ULONG Pid)
{
	PEPROCESS Process;
	NTSTATUS status;

	if (Pid == 0 || Pid == 4) {
		DbgPrint("[-] Refusing to terminate a critical system process (PID: %lu)\n", Pid);
		return STATUS_INVALID_PARAMETER;
	}

	status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)Pid, &Process);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[-] Failed to lookup process by PID: %lu\n", Pid);
		return status;
	}

	PVOID baseAddress = g_PsGetProcessSectionBaseAddress(Process);

	status = g_MmUnmapViewOfSection(Process, baseAddress);

	if (!NT_SUCCESS(status)) {
		DbgPrint("[-] Failed to unmap view of section for PID: %lu, status: 0x%08X\n", Pid, status);
		ObDereferenceObject(Process);
		return status;
	}

	DbgPrint("[+] Successfully unmapped view of section for PID: %lu\n", Pid);

	ObDereferenceObject(Process);

	return status;
}

NTSTATUS DeviceIoControl(PDEVICE_OBJECT, PIRP Irp) {
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_TERM_3) {
		if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(PROC_TERM)) {
			status = STATUS_INVALID_PARAMETER;
		}
		else {
			PPROC_TERM ProcTerm = (PPROC_TERM)Irp->AssociatedIrp.SystemBuffer;
			if (ProcTerm) {
				status = KillProcessByPid(ProcTerm->Pid);
			}
			else {
				status = STATUS_INVALID_PARAMETER;
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
