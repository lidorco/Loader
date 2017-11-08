//This code has taken from: 
//    https://www.codeproject.com/Tips/139349/Getting-the-address-of-a-function-in-a-DLL-loaded
//I added LoadModuleRemoteProcess function for internal use.

#pragma once

// Windows Header Files:
#include <windows.h>
#include <commctrl.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


HMODULE WINAPI GetRemoteModuleHandle(HANDLE hProcess, LPCSTR lpModuleName);
FARPROC WINAPI GetRemoteProcAddress(HANDLE hProcess, HMODULE hModule, LPCSTR lpProcName, UINT Ordinal = 0, BOOL UseOrdinal = FALSE);
HMODULE WINAPI LoadModuleRemoteProcess(HANDLE hProcess, LPCSTR lpModuleName);