#pragma once

#include "PeFile.h"

typedef BOOL(WINAPI *DllEntryProc)(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpReserved);


class Loader {

public:
	Loader(PeFile*);
	int load(); 
	int attach();
	PDWORD getLoadedFunctionByName(char*) const;

private:
	PeFile* m_pe_file;
	DWORD m_allocated_base_address;

	int allocateMemory();
	int loadSections() const;
	int handleRelocations() const;
	int resolveImports();
	int protectMemory() const;
	static DWORD getImportedFunctionAddress(char*, char*);
};
