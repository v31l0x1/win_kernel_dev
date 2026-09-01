#include <ntifs.h>
#include <ntstatus.h>

#pragma warning(disable: 4996) // warning C4996: ''

#define IOCTL_TERMINATE_PROC CTL_CODE( 0x8001, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )

typedef unsigned long DWORD;

typedef NTSTATUS(NTAPI* PS_TERMINATE_PROCESS)(PEPROCESS Process, NTSTATUS ExitStatus);
PS_TERMINATE_PROCESS g_pPsTerminateProcess = NULL;

typedef struct {
    DWORD Pid;
} ProcTerm;

typedef struct _TERM_THREAD_CONTEXT {
    PEPROCESS TargetProcess;
    NTSTATUS ExitStatus;
} TERM_THREAD_CONTEXT, * PTERM_THREAD_CONTEXT;

NTSTATUS DeviceCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS GetPsTerminateProcessAddress();
VOID UnloadDriver(PDRIVER_OBJECT DriverObject);
VOID TerminateSystemThread(PVOID Context);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
    UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\PsTerminate");
    UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\PsTerminate");
    NTSTATUS ntStatus;
    PDEVICE_OBJECT DeviceObject = NULL;

    ntStatus = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceObject);
    if (!NT_SUCCESS(ntStatus)) {
        DbgPrint("[-] Failed to create device");
        return ntStatus;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
    DriverObject->DriverUnload = UnloadDriver;

    ntStatus = GetPsTerminateProcessAddress();
    if (!NT_SUCCESS(ntStatus)) {
        DbgPrint("[-] Failed to resolve PsTerminateProcess");
        IoDeleteDevice(DeviceObject);
        return ntStatus;
    }

    ntStatus = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);
    if (!NT_SUCCESS(ntStatus)) {
        DbgPrint("[-] Failed to create symbolic link");
        IoDeleteDevice(DeviceObject);
        return ntStatus;
    }

    return STATUS_SUCCESS;
}

VOID UnloadDriver(PDRIVER_OBJECT DriverObject)
{
    PDEVICE_OBJECT DeviceObject = DriverObject->DeviceObject;
    UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\??\\PsTerminate");

    IoDeleteSymbolicLink(&SymbolicLinkName);
    if (DeviceObject != NULL) {
        IoDeleteDevice(DeviceObject);
    }
    DbgPrint("[+] Driver unloaded successfully");
}

NTSTATUS GetPsTerminateProcessAddress()
{
    UNICODE_STRING routineName = RTL_CONSTANT_STRING(L"PsTerminateProcess");
    g_pPsTerminateProcess = (PS_TERMINATE_PROCESS)MmGetSystemRoutineAddress(&routineName);

    if (g_pPsTerminateProcess == NULL) {
        DbgPrint("[-] Failed to find PsTerminateProcess address");
        return STATUS_NOT_FOUND;
    }

    DbgPrint("[+] PsTerminateProcess found at 0x%p", g_pPsTerminateProcess);
    return STATUS_SUCCESS;
}

VOID TerminateSystemThread(PVOID Context)
{
    PTERM_THREAD_CONTEXT ctx = (PTERM_THREAD_CONTEXT)Context;

    if (g_pPsTerminateProcess && ctx->TargetProcess) {
        g_pPsTerminateProcess(ctx->TargetProcess, ctx->ExitStatus);
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DeviceCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(DeviceObject);

    if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_TERMINATE_PROC) {

        ProcTerm* procTerm = (ProcTerm*)Irp->AssociatedIrp.SystemBuffer;

        if (procTerm != NULL && irpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(ProcTerm)) {

            PEPROCESS pProcess = NULL;
            CLIENT_ID clientId;
            clientId.UniqueProcess = UlongToHandle(procTerm->Pid);
            clientId.UniqueThread = NULL;

            ntStatus = PsLookupProcessByProcessId(clientId.UniqueProcess, &pProcess);

            if (NT_SUCCESS(ntStatus)) {

                PTERM_THREAD_CONTEXT ctx = (PTERM_THREAD_CONTEXT)ExAllocatePoolWithTag(NonPagedPool, sizeof(TERM_THREAD_CONTEXT), 'TerP');
                if (ctx) {
                    ctx->TargetProcess = pProcess;
                    ctx->ExitStatus = STATUS_SUCCESS;

                    HANDLE hThread = NULL;
                    KAPC_STATE apcState;

                    KeStackAttachProcess(pProcess, &apcState);

                    ntStatus = PsCreateSystemThread(
                        &hThread,
                        THREAD_ALL_ACCESS,
                        NULL,
                        NtCurrentProcess(),
                        NULL,
                        TerminateSystemThread,
                        ctx
                    );

                    KeUnstackDetachProcess(&apcState);

                    if (NT_SUCCESS(ntStatus)) {
                        // 4. Wait for the termination thread to finish
                        ZwWaitForSingleObject(hThread, FALSE, NULL);
                        ZwClose(hThread);
                        DbgPrint("[+] Successfully terminated PID: %lu via PsTerminateProcess", procTerm->Pid);
                    }
                    else {
                        DbgPrint("[-] Failed to create system thread in target process");
                        ExFreePoolWithTag(ctx, 'TerP');
                    }
                }
                else {
                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                }

                ObDereferenceObject(pProcess);
            }
            else {
                DbgPrint("[-] Failed to lookup PID: %lu", procTerm->Pid);
            }
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