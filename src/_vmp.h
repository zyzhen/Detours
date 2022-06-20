#pragma once

//void RestoreZwProtectVirtualMemory();
//#undef VirtualProtect
BOOL VirtualProtectVMP(
	_In_  LPVOID lpAddress,
	_In_  SIZE_T dwSize,
	_In_  DWORD flNewProtect,
	_Out_ PDWORD lpflOldProtect
);
BOOL VirtualProtectExVMP(
    _In_  HANDLE hProcess,
    _In_  LPVOID lpAddress,
    _In_  SIZE_T dwSize,
    _In_  DWORD flNewProtect,
    _Out_ PDWORD lpflOldProtect
);
BOOL WriteProcessMemoryVMP(
    HANDLE hProcess
    , LPVOID lpBaseAddress
    , LPCVOID lpBuffer
    , SIZE_T nSize
    , SIZE_T* lpNumberOfBytesWritten);