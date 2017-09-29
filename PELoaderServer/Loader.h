#pragma once

#include "PeFile.h"

typedef BOOL(WINAPI *DllEntryProc)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);


class Loader {

public:
	Loader(PeFile*);
	int Load(); 
	int Attach();
	PDWORD GetLoudedFunctionByName(char*);

private:
	PeFile* pe_file_;
	DWORD allocated_base_addr_;

	int AllocateMemory();
	int LoadSections();
	int HandleRelocations();
	int ResolveImports();
	int ProtectMemory();
	DWORD GetImportedFunctionAddress(char*, char*);
};
