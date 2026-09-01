#include <ntddk.h>
#include <ntstatus.h>

#pragma warning(disable: 4996)

#define IOCTL_TERM	CTL_CODE(0x8000, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define MAX_PROCESS_NAME 260

typedef unsigned char BYTE;
typedef unsigned long DWORD;

NTSTATUS CreateCloseFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID UnloadDriver(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#pragma pack(push, 1)
typedef struct {
	UINT32 Hash;
} IOCTLStruct;
#pragma pack(pop)

typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemInformationClassMin = 0,
	SystemBasicInformation = 0,
	SystemProcessorInformation = 1,
	SystemPerformanceInformation = 2,
	SystemTimeOfDayInformation = 3,
	SystemPathInformation = 4,
	SystemNotImplemented1 = 4,
	SystemProcessInformation = 5,
	SystemProcessesAndThreadsInformation = 5,
	SystemCallCountInfoInformation = 6,
	SystemCallCounts = 6,
	SystemDeviceInformation = 7,
	SystemConfigurationInformation = 7,
	SystemProcessorPerformanceInformation = 8,
	SystemProcessorTimes = 8,
	SystemFlagsInformation = 9,
	SystemGlobalFlag = 9,
	SystemCallTimeInformation = 10,
	SystemNotImplemented2 = 10,
	SystemModuleInformation = 11,
	SystemLocksInformation = 12,
	SystemLockInformation = 12,
	SystemStackTraceInformation = 13,
	SystemNotImplemented3 = 13,
	SystemPagedPoolInformation = 14,
	SystemNotImplemented4 = 14,
	SystemNonPagedPoolInformation = 15,
	SystemNotImplemented5 = 15,
	SystemHandleInformation = 16,
	SystemObjectInformation = 17,
	SystemPageFileInformation = 18,
	SystemPagefileInformation = 18,
	SystemVdmInstemulInformation = 19,
	SystemInstructionEmulationCounts = 19,
	SystemVdmBopInformation = 20,
	SystemInvalidInfoClass1 = 20,
	SystemFileCacheInformation = 21,
	SystemCacheInformation = 21,
	SystemPoolTagInformation = 22,
	SystemInterruptInformation = 23,
	SystemProcessorStatistics = 23,
	SystemDpcBehaviourInformation = 24,
	SystemDpcInformation = 24,
	SystemFullMemoryInformation = 25,
	SystemNotImplemented6 = 25,
	SystemLoadImage = 26,
	SystemUnloadImage = 27,
	SystemTimeAdjustmentInformation = 28,
	SystemTimeAdjustment = 28,
	SystemSummaryMemoryInformation = 29,
	SystemNotImplemented7 = 29,
	SystemNextEventIdInformation = 30,
	SystemNotImplemented8 = 30,
	SystemEventIdsInformation = 31,
	SystemNotImplemented9 = 31,
	SystemCrashDumpInformation = 32,
	SystemExceptionInformation = 33,
	SystemCrashDumpStateInformation = 34,
	SystemKernelDebuggerInformation = 35,
	SystemContextSwitchInformation = 36,
	SystemRegistryQuotaInformation = 37,
	SystemLoadAndCallImage = 38,
	SystemPrioritySeparation = 39,
	SystemPlugPlayBusInformation = 40,
	SystemNotImplemented10 = 40,
	SystemDockInformation = 41,
	SystemNotImplemented11 = 41,
	SystemInvalidInfoClass2 = 42,
	SystemProcessorSpeedInformation = 43,
	SystemInvalidInfoClass3 = 43,
	SystemCurrentTimeZoneInformation = 44,
	SystemTimeZoneInformation = 44,
	SystemLookasideInformation = 45,
	SystemSetTimeSlipEvent = 46,
	SystemCreateSession = 47,
	SystemDeleteSession = 48,
	SystemInvalidInfoClass4 = 49,
	SystemRangeStartInformation = 50,
	SystemVerifierInformation = 51,
	SystemAddVerifier = 52,
	SystemSessionProcessesInformation = 53,
	SystemInformationClassMax
} SYSTEM_INFORMATION_CLASS;
typedef struct _SYSTEM_THREAD_INFORMATION {
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER CreateTime;
	ULONG WaitTime;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	ULONG ThreadState;
	KWAIT_REASON WaitReason;
} SYSTEM_THREAD_INFORMATION, * PSYSTEM_THREAD_INFORMATION;
typedef struct _SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER WorkingSetPrivateSize;
	ULONG HardFaultCount;
	ULONG NumberOfThreadsHighWatermark;
	ULONGLONG CycleTime;
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR UniqueProcessKey;
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	SYSTEM_THREAD_INFORMATION Threads[1];
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;

extern "C"
{
	NTSTATUS ZwQuerySystemInformation(SYSTEM_INFORMATION_CLASS systemInformationClass, PVOID systemInformation, ULONG systemInformationLength, PULONG returnLength);
}

//UINT32 HashStringDjb2aW(PWCHAR String)
//{
//	UINT32 Hash = 5381;
//	UCHAR c = 0;
//
//	while ((c = (BYTE)*String++))
//		Hash = ((Hash << 5) + Hash) ^ c;
//
//	return Hash;
//}

UINT32 HashStringDjb2aW(PWCHAR String)
{
	UINT32 Hash = 5381;
	UCHAR c = 0;
	UCHAR lowerChar = 0;

	while ((c = (BYTE)*String++)) {
		if (c >= 'A' && c <= 'Z') {
			lowerChar = c + 0x20;
		}
		else {
			lowerChar = c;
		}
		Hash = ((Hash << 5) + Hash) ^ lowerChar;
	}

	return Hash;
}

DWORD findProcId(UINT32 procHash)
{
	NTSTATUS status;
	PVOID buffer = NULL;
	ULONG bufferSize = 0;
	BOOLEAN found = FALSE;
	PSYSTEM_PROCESS_INFORMATION spi = NULL;
	//UNICODE_STRING targetName;

	DWORD pid = 0;

	//RtlInitUnicodeString(&targetName, executable_name);

	status = ZwQuerySystemInformation(SystemProcessInformation, NULL, 0, &bufferSize);
	if (status != STATUS_INFO_LENGTH_MISMATCH)
		return pid;

	buffer = ExAllocatePoolWithTag(NonPagedPool, bufferSize, 'proc');
	if (!buffer)
		return pid;

	status = ZwQuerySystemInformation(SystemProcessInformation, buffer, bufferSize, &bufferSize);
	if (!NT_SUCCESS(status)) {
		ExFreePoolWithTag(buffer, 'proc');
		return pid;
	}

	spi = (PSYSTEM_PROCESS_INFORMATION)buffer;
	while (TRUE) {
		if (spi->UniqueProcessId && spi->ImageName.Buffer) {
			UNICODE_STRING currentName;
			RtlInitUnicodeString(&currentName, spi->ImageName.Buffer);

			// if (RtlCompareUnicodeString(&currentName, &targetName, FALSE) == 0) {
			DbgPrint("Process Name: %wZ, Hash: %u\n", currentName, HashStringDjb2aW(spi->ImageName.Buffer));
			if (HashStringDjb2aW(spi->ImageName.Buffer) == procHash) {
				pid = (DWORD)(ULONG_PTR)spi->UniqueProcessId;
				DbgPrint("Found Process: %wZ, PID: %d\n", currentName, pid);
				found = TRUE;
				break;
			}
		}

		if (spi->NextEntryOffset == 0)
			break;

		spi = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)spi + spi->NextEntryOffset);
	}

	ExFreePoolWithTag(buffer, 'proc');

	return pid;
}

extern "C" {
	NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
	{
		NTSTATUS ntStatus;
		UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\NoSense");
		UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\NoSense");
		PDEVICE_OBJECT deviceObject = NULL;
		UNREFERENCED_PARAMETER(RegistryPath);

		ntStatus = IoCreateDevice(
			DriverObject,
			0,
			&DeviceName,
			FILE_DEVICE_UNKNOWN,
			FILE_DEVICE_SECURE_OPEN,
			FALSE,
			&deviceObject);
		if (!NT_SUCCESS(ntStatus)) {
			DbgPrint("Couldn't create device\n");
			//IoDeleteDevice(deviceObject);
			return ntStatus;
		}

		DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseFunction;
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseFunction;
		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;
		DriverObject->DriverUnload = UnloadDriver;

		ntStatus = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);

		UNICODE_STRING notepad = RTL_CONSTANT_STRING(L"notepad.exe");
		DbgPrint("Hash of notepad.exe: 0x%08X\n", HashStringDjb2aW(notepad.Buffer));
		
		if (!NT_SUCCESS(ntStatus)) {
			DbgPrint("Couldn't create symbolic link\n");
			IoDeleteDevice(deviceObject);
		}

		return ntStatus;
	}
}

NTSTATUS CreateCloseFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

VOID UnloadDriver(_In_ PDRIVER_OBJECT DriverObject) {
	PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;

	UNICODE_STRING SymbolicLink = RTL_CONSTANT_STRING(L"\\??\\NoSense");

	IoDeleteSymbolicLink(&SymbolicLink);

	if (deviceObject != NULL)
	{
		IoDeleteDevice(deviceObject);
	}

	DbgPrint("Driver Unloaded!");
}

NTSTATUS DeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS ntStatus = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(DeviceObject);

	if (irpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_TERM) {
		IOCTLStruct* inputBuffer = (IOCTLStruct*)Irp->AssociatedIrp.SystemBuffer;

		if (inputBuffer != NULL) {
			 //Print the process Name
			DbgPrint("Process Name: 0X%08X\n", inputBuffer->Hash);
			DWORD pid;
			pid = findProcId(inputBuffer->Hash);
			if (pid > 0) {
				DbgPrint("Process ID: %d\n", pid);

				HANDLE procHandle;
				OBJECT_ATTRIBUTES objAttr;
				CLIENT_ID clientId;
				clientId.UniqueProcess = (HANDLE)(ULONG_PTR)pid;
				clientId.UniqueThread = NULL;
				InitializeObjectAttributes(&objAttr, NULL, 0, NULL, NULL);

				ntStatus = ZwOpenProcess(&procHandle, 1u, &objAttr, &clientId);
				if (NT_SUCCESS(ntStatus)) {
					ntStatus = ZwTerminateProcess(procHandle, 0);
					ZwClose(procHandle);
				}
				else {
					DbgPrint("Couldn't open process with PID %d\n", pid);
				}
				ntStatus = STATUS_SUCCESS;
			}
			else {
				//DbgPrint("Couldn't find process with hash %u\n", inputBuffer->Hash);
				ntStatus = STATUS_NOT_FOUND;
			}
		}
		else {
			ntStatus = STATUS_INVALID_PARAMETER;
		}
	}
	else {
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
	}

	Irp->IoStatus.Status = ntStatus;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return ntStatus;
}