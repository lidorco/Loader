#pragma once

#include "PeFile.h"
#include "RemoteModules.h"

class RemoteDllLoader {

public:
	RemoteDllLoader(DWORD, PeFile*);
	~RemoteDllLoader();
	int Load();
	int Attach();
	int CallLoudedFunctionByName(LPCSTR);

private:
	HANDLE remote_process_;
	PeFile* pe_file_;
	DWORD allocated_base_addr_;

	int AllocateMemory();
	int LoadSections();
	int HandleRelocations();
	int ResolveImports();
	int ProtectMemory();
};
