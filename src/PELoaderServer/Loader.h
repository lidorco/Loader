#pragma once

#include "PeFile.h"

typedef BOOL(WINAPI *DllEntryProc)(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpReserved);


class Loader {

public:
	Loader(PeFile*);
	int load(); 
	int attach() const;
	PDWORD getLoadedFunctionByName(char*) const;

private:
	PeFile* m_pe_file;
	DWORD m_allocated_base_address;

	int allocateMemory();
	int loadSections();
	int handleRelocations();
	int resolveImports();
	int protectMemory();
	DWORD getImportedFunctionAddress(char*, char*);
};
