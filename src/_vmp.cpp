// #include "pch.h"
#include <windows.h>
# include <tchar.h>
//# include <ntstatus.h>
typedef LONG NTSTATUS;
#define STATUS_INVALID_PAGE_PROTECTION 0xC0000045
//# include "ntdll.h"
#include "_vmp.h"


BYTE* g_pAddr = NULL;
typedef SIZE_T(CALLBACK* PZwProtectVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID* BaseAddress,
    SIZE_T* NumberOfBytesToProtect,
    ULONG NewAccessProtection,
    PULONG OldAccessProtection);
PZwProtectVirtualMemory pZwProtectVirtualMemory;
LONG Init32()
{
    if (g_pAddr != NULL)
    {
        return 0;
    }
    SIZE_T ntdllAddr = (SIZE_T)GetModuleHandleA("ntdll.dll");
    SIZE_T apiAddr = (SIZE_T)GetProcAddress((HMODULE)ntdllAddr, "ZwProtectVirtualMemory");
    if (apiAddr == NULL || ntdllAddr == NULL)
    {
        //MessageBox(0, _T("LoadLibrary|GetProcAddress,Error"), _T("error"), MB_OK);
        //CloseHandle(hFile);
        return 1;
    }

	//��Ntdll������ڴ�?
	HANDLE hFile = CreateFile(_T("C:\\Windows\\System32\\ntdll.dll"), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		//MessageBox(0, _T("CreateFileError"), _T("error"), MB_OK);
		return 2;
	}
    DWORD dwHighSize = 0;
    DWORD dwLowSize = GetFileSize(hFile, &dwHighSize);
    BYTE* pBuff = new BYTE[dwLowSize + dwHighSize]();
    DWORD  dwFileSize = 0;
	ReadFile(hFile, pBuff, dwLowSize + dwHighSize, &dwFileSize, NULL);
	CloseHandle(hFile);

    //Ѱ����������
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pBuff;
    PIMAGE_NT_HEADERS32 pNtHeader = (PIMAGE_NT_HEADERS32)(pDosHeader->e_lfanew + ((SIZE_T)pDosHeader));
    PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(((SIZE_T)(pNtHeader)) + sizeof(IMAGE_NT_HEADERS32));
    SIZE_T Deviation = 0;
    //ZwProtectVirtualMemory ��ַ - ntdll��ַ = RVA
    SIZE_T Rva = apiAddr - ntdllAddr;

    // ��ͨ��RVA�ҵ���������,����RVA-��ǰ����RVA+��ǰ�����ļ�ƫ�� = F0A
    // �����F0A�ҵ���Ե�opcode,���Ƶ����ڴ�ִ������ת
    for (WORD i = 0; i < pNtHeader->FileHeader.NumberOfSections; ++i)
    {
        if (Rva > pSectionHeader[i].VirtualAddress && Rva < pSectionHeader[i].VirtualAddress + pSectionHeader[i].SizeOfRawData)
        {//ͨ��RVA�ҵ���������,����RVA-��ǰ����RVA+�ļ�ƫ�� = F0A
            Deviation = Rva - pSectionHeader[i].VirtualAddress + pSectionHeader[i].PointerToRawData;
            break;
        }
    }

    if (Deviation == 0)
    {
        //MessageBoxA(0, "�������ƫ�ƴ���?", "error", MB_OK);
        delete[] pBuff;
        //CloseHandle(hFile);
        return 3;
    }
    // ����ö���ƫ���ҵ���Ե�opcode
    g_pAddr = (BYTE*)VirtualAlloc(0, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);//����һ�ζѿռ�ִ�б��޸ĵĻ�����
    if (g_pAddr == NULL)
    {
        // MessageBoxA(0, "VirtualAlloc Error", "error", MB_OK);
        delete[] pBuff;
        //CloseHandle(hFile);
        return 4;
    }
    memcpy(g_pAddr, pBuff + Deviation, 5);//x32��VMP�޸���ǰ5�ֽ�,���ļ��ڵ�ԭ5�ֽڿ�����ȥ
    g_pAddr[5] = 0x68;//push
    apiAddr += 5;
    memcpy(g_pAddr + 6, &apiAddr, 4);//push�ĵ�ַ��ZwProtectVirtualMemory+5�ĵط�
    g_pAddr[10] = 0xc3;//ʹ��retn��ȥ
    delete[] pBuff;
    return 0;
}
SIZE_T MyZwProtectVirtualMemory(
    HANDLE ProcessHandle, //���̾��?
    PVOID* BaseAddress,//��ַ��ָ��
    SIZE_T* NumberOfBytesToProtect,//�޸ĵĴ�С,�������ú��ĳɳɹ��޸ĵĴ�С
    ULONG NewAccessProtection,//�µ��ڴ�����
    PULONG OldAccessProtection)//�ɵ��ڴ�����
{

#ifdef _WIN64
    Init64();
#else
    LONG err = Init32();
#endif
    if (err != NO_ERROR)
    {
        return err;
    }
    pZwProtectVirtualMemory = (PZwProtectVirtualMemory)g_pAddr;
    return pZwProtectVirtualMemory(ProcessHandle, BaseAddress, NumberOfBytesToProtect, NewAccessProtection, OldAccessProtection);

}

//BOOL VirtualProtectVMP(
//    _In_  LPVOID lpAddress,
//    _In_  SIZE_T dwSize,
//    _In_  DWORD flNewProtect,
//    _Out_ PDWORD lpflOldProtect
//)
//{
//    //VirtualProtect();
//    LONG err = MyZwProtectVirtualMemory((HANDLE)-1, &lpAddress
//        , &dwSize, flNewProtect, lpflOldProtect);
//    BOOL flag = FALSE;
//    if (err == NO_ERROR)
//    {
//        flag = TRUE;
//    }
//    else if (err == ERROR_ACCESS_DENIED)
//    {
//        if (ERROR_MOD_NOT_FOUND == GetLastError())
//            flag = TRUE;
//    }
//
//    if (flag)
//        SetLastError(NO_ERROR);
//    return flag;
//     //return TRUE;
//}
typedef BOOL(WINAPI * RtlFlushSecureMemoryCache_t)(PVOID BaseAddress, SIZE_T NumberOfBytesToProtect);
typedef BOOL(__fastcall * BaseSetLastNTError_t)(NTSTATUS Status);

BOOL RtlFlushSecureMemoryCache(PVOID BaseAddress, SIZE_T NumberOfBytesToProtect)
{
    static RtlFlushSecureMemoryCache_t RtlFlushSecureMemoryCache_f = []
    {
        return (RtlFlushSecureMemoryCache_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlFlushSecureMemoryCache");
    }();
    return RtlFlushSecureMemoryCache_f(BaseAddress, NumberOfBytesToProtect);
}
BOOL BaseSetLastNTError(NTSTATUS Status)
{
    static BaseSetLastNTError_t BaseSetLastNTError_f = []
    {
        return (BaseSetLastNTError_t)GetProcAddress(GetModuleHandleA("KernelBase.dll"), "BaseSetLastNTError");
    }();

    return BaseSetLastNTError_f(Status);
}

BOOL VirtualProtectVMP(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect)
{
    NTSTATUS v4; // eax
    NTSTATUS v5; // esi
    PVOID BaseAddress; // [esp+4h] [ebp-8h] BYREF
    SIZE_T NumberOfBytesToProtect; // [esp+8h] [ebp-4h] BYREF

    NumberOfBytesToProtect = dwSize;
    BaseAddress = lpAddress;
    v4 = MyZwProtectVirtualMemory((HANDLE)0xFFFFFFFF, &BaseAddress, &NumberOfBytesToProtect, flNewProtect, lpflOldProtect);
    v5 = v4;
    if (v4 >= 0)
        return TRUE;
    //STATUS_ACCESS_DENIED;

    if (v4 == STATUS_INVALID_PAGE_PROTECTION)
    {
        //if (RtlFlushSecureMemoryCache(BaseAddress, NumberOfBytesToProtect))
        if (RtlFlushSecureMemoryCache(BaseAddress, NumberOfBytesToProtect))
        {
            v5 = MyZwProtectVirtualMemory(
                (HANDLE)0xFFFFFFFF,
                &BaseAddress,
                &NumberOfBytesToProtect,
                flNewProtect,
                lpflOldProtect);
            if (v5 >= 0)
                return 1;
        }
    }
    BaseSetLastNTError(v5);
    return 0;
}

BOOL VirtualProtectExVMP(_In_ HANDLE hProcess, _In_ LPVOID lpAddress, _In_ SIZE_T dwSize, _In_ DWORD flNewProtect, _Out_ PDWORD lpflOldProtect)
{
    NTSTATUS v5; // eax
    int v6; // esi

    v5 = MyZwProtectVirtualMemory(hProcess, &lpAddress, &dwSize, flNewProtect, lpflOldProtect);
    v6 = v5;
    if (v5 >= 0)
        return 1;
    if (v5 == STATUS_INVALID_PAGE_PROTECTION && hProcess == (HANDLE)0xFFFFFFFF)
    {
        if (RtlFlushSecureMemoryCache(lpAddress, dwSize))
        {
            v6 = MyZwProtectVirtualMemory((HANDLE)0xFFFFFFFF
                , &lpAddress, &dwSize, flNewProtect, lpflOldProtect);
            if (v6 >= 0)
                return 1;
        }
    }
    BaseSetLastNTError(v6);
    return 0;
}

typedef enum _MEMORY_INFORMATION_CLASS
{
    MemoryBasicInformation, // MEMORY_BASIC_INFORMATION
    MemoryWorkingSetInformation, // MEMORY_WORKING_SET_INFORMATION
    MemoryMappedFilenameInformation, // UNICODE_STRING
    MemoryRegionInformation, // MEMORY_REGION_INFORMATION
    MemoryWorkingSetExInformation, // MEMORY_WORKING_SET_EX_INFORMATION
    MemorySharedCommitInformation, // MEMORY_SHARED_COMMIT_INFORMATION
    MemoryImageInformation, // MEMORY_IMAGE_INFORMATION
    MemoryRegionInformationEx,
    MemoryPrivilegedBasicInformation
} MEMORY_INFORMATION_CLASS;

typedef NTSTATUS(* NtQueryVirtualMemory_t) (
    HANDLE                   ProcessHandle,
    PVOID                    BaseAddress,
    MEMORY_INFORMATION_CLASS MemoryInformationClass,
    PVOID                    MemoryInformation,
    SIZE_T                   MemoryInformationLength,
    PSIZE_T                  ReturnLength
);
NTSTATUS NtQueryVirtualMemory(
    HANDLE                   ProcessHandle,
    PVOID                    BaseAddress,
    MEMORY_INFORMATION_CLASS MemoryInformationClass,
    PVOID                    MemoryInformation,
    SIZE_T                   MemoryInformationLength,
    PSIZE_T                  ReturnLength
)
{
    static NtQueryVirtualMemory_t NtQueryVirtualMemory_f = []
    {
        return (NtQueryVirtualMemory_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryVirtualMemory");
    }();

    return NtQueryVirtualMemory_f(ProcessHandle, BaseAddress, MemoryInformationClass
        , MemoryInformation, MemoryInformationLength, ReturnLength);
}

//typedef NTSTATUS (*NtQueryVirtualMemory_t)(
//    [in]            HANDLE                   ProcessHandle,
//    [in, optional]  PVOID                    BaseAddress,
//    [in]            MEMORY_INFORMATION_CLASS MemoryInformationClass,
//    [out]           PVOID                    MemoryInformation,
//    [in]            SIZE_T                   MemoryInformationLength,
//    [out, optional] PSIZE_T                  ReturnLength
//);
//NTSTATUS NtQueryVirtualMemory(
//    [in]            HANDLE                   ProcessHandle,
//    [in, optional]  PVOID                    BaseAddress,
//    [in]            MEMORY_INFORMATION_CLASS MemoryInformationClass,
//    [out]           PVOID                    MemoryInformation,
//    [in]            SIZE_T                   MemoryInformationLength,
//    [out, optional] PSIZE_T                  ReturnLength
//)
//{
//    static NtQueryVirtualMemory_t NtQueryVirtualMemory_f = []
//    {
//        return (NtQueryVirtualMemory_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryVirtualMemory");
//    }();
//
//    return NtQueryVirtualMemory_f(ProcessHandle, BaseAddress, MemoryInformationClass
//        , MemoryInformation, MemoryInformationLength, ReturnLength);
//}

typedef int (*NtWriteVirtualMemory_t)(HANDLE a1, LPVOID a2, LPVOID a3, int a4, ULONG_PTR* a5);
int NtWriteVirtualMemory(HANDLE a1, LPVOID a2, LPVOID a3, int a4, ULONG_PTR* a5)
{
    static NtWriteVirtualMemory_t NtWriteVirtualMemory_f = []
    {
        return (NtWriteVirtualMemory_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWriteVirtualMemory");
    }();

    return NtWriteVirtualMemory_f(a1,a2,a3,a4,a5);
}
typedef int (*ZwFlushInstructionCache_t)(HANDLE a1, LPVOID a2, SIZE_T a3);
int NtFlushInstructionCache(HANDLE a1, LPVOID a2, SIZE_T a3)
{
    static ZwFlushInstructionCache_t ZwFlushInstructionCache_f = []
    {
        return (ZwFlushInstructionCache_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "ZwFlushInstructionCache");
    }();

    return ZwFlushInstructionCache_f(a1, a2, a3);
}
BOOL WriteProcessMemoryVMP(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten)
{
#define Size 0x1000
    char v5; // bl
    NTSTATUS v6; // esi
    NTSTATUS v7; // eax
    int v9; // ecx
    int VirtualMemoryInformation[7]; // [esp+Ch] [ebp-30h] BYREF
    ULONG_PTR NumberOfBytesWritten; // [esp+28h] [ebp-14h] BYREF
    ULONG NewAccessProtection; // [esp+2Ch] [ebp-10h]
    PVOID BaseAddress; // [esp+30h] [ebp-Ch] BYREF
    ULONG OldAccessProtection; // [esp+34h] [ebp-8h] BYREF
    SIZE_T NumberOfBytesToProtect; // [esp+38h] [ebp-4h] BYREF

    v5 = 0;
    OldAccessProtection = 0;
    BaseAddress = 0;
    NumberOfBytesToProtect = 0;
    v6 = NtQueryVirtualMemory(hProcess, lpBaseAddress, MemoryPrivilegedBasicInformation, VirtualMemoryInformation, 0x1Cu, 0);
    if (v6 < 0)
        goto LABEL_11;
    if ((VirtualMemoryInformation[5] & 0xCC) != 0)
        goto LABEL_3;
    if ((VirtualMemoryInformation[5] & 0x30) == 0)
        goto LABEL_21;
    if (VirtualMemoryInformation[6] != 0x1000000)
    {
        if (VirtualMemoryInformation[6] == 0x20000)
        {
            v9 = 0x20000040;
            goto LABEL_17;
        }
    LABEL_21:
        v6 = 0xC0000005;
        goto LABEL_11;
    }
    v9 = 0x20000080;
LABEL_17:
    NewAccessProtection = v9 | VirtualMemoryInformation[5] & 0xFFFFFFCF;
    BaseAddress = (PVOID)VirtualMemoryInformation[0];
    NumberOfBytesToProtect = ~(Size - 1) & (nSize - 1 + Size + ((unsigned int)lpBaseAddress & (Size - 1)));
    if (NumberOfBytesToProtect > (SIZE_T)VirtualMemoryInformation[3])
        NumberOfBytesToProtect = VirtualMemoryInformation[3];
    v6 = MyZwProtectVirtualMemory(
        hProcess,
        &BaseAddress,
        &NumberOfBytesToProtect,
        NewAccessProtection,
        &OldAccessProtection);
    if (v6 < 0)
        goto LABEL_11;
    v5 = 1;
LABEL_3:
    v7 = NtWriteVirtualMemory(hProcess, lpBaseAddress, (PVOID)lpBuffer, nSize, &NumberOfBytesWritten);
    v6 = v7;
    if (lpNumberOfBytesWritten)
        *lpNumberOfBytesWritten = NumberOfBytesWritten;
    if (v7 >= 0)
        NtFlushInstructionCache(hProcess, lpBaseAddress, nSize);
    if (v5)
        v6 = MyZwProtectVirtualMemory(
            hProcess,
            &BaseAddress,
            &NumberOfBytesToProtect,
            OldAccessProtection | 0x20000000,
            &OldAccessProtection);
    if (v6 >= 0)
        return 1;
LABEL_11:
    BaseSetLastNTError(v6);
    return 0;
}

//void RestoreZwProtectVirtualMemory()
//{
//
//    PVOID addr = (PVOID)GetProcAddress(GetModuleHandleA("ntdll.dll"), "ZwProtectVirtualMemory");
//    DWORD size = 5;
//    DWORD OldProtect;
//    if (1)
//    {
//        BOOL flag;
//        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, 0x3680);
//        flag = MyZwProtectVirtualMemory(hProcess, &addr, &size, PAGE_READWRITE, &OldProtect);//�޸�����
//       
//        flag = WriteProcessMemory(hProcess, addr, g_pAddr, 5, &size);
//        size = 5;
//        flag = MyZwProtectVirtualMemory(hProcess, &addr, &size, OldProtect, &OldProtect);//�޸�����
//    }
//    else
//    {
//        MyZwProtectVirtualMemory((HANDLE)-1, &addr, &size, PAGE_READWRITE, &OldProtect);//�޸�����
//        memcpy(addr, g_pAddr, 5);
//
//        size = 5;
//        MyZwProtectVirtualMemory((HANDLE)-1, &addr, &size, OldProtect, &OldProtect);//�޸�����
//    }
//
//
//
//}