
#include "RemoteDllLoader.h"
#include "Debug.h"


RemoteDllLoader::RemoteDllLoader(DWORD pid, PeFile* pe_obj)
{
	allocated_base_addr_ = NULL;
	pe_file_ = pe_obj;
	remote_process_ = OpenProcess(PROCESS_ALL_ACCESS, FALSE | PROCESS_VM_OPERATION, pid);
	
	if (NULL == remote_process_)
	{
		LOG("Couldn't create handle to remote process RemoteDllLoader::RemoteDllLoader : " + std::to_string(GetLastError()));
	}
}

RemoteDllLoader::~RemoteDllLoader()
{
	CloseHandle(remote_process_);
}

int RemoteDllLoader::Load()
{
	AllocateMemory();
	LoadSections();
	HandleRelocations();
	ResolveImports();
	ProtectMemory();
	return 0;
}

int RemoteDllLoader::Attach()
{
	/*
	DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
	Attach() wont's move any parameters to remote process.
	*/
	if (!CreateRemoteThread(remote_process_, NULL, NULL,
		(LPTHREAD_START_ROUTINE)(allocated_base_addr_ + pe_file_->nt_header.OptionalHeader.AddressOfEntryPoint),
		NULL, NULL, NULL))
	{
		LOG("Error calling to DllMain in remote thread: " + std::to_string(GetLastError()));
		return GetLastError();
	}
	return 0;
}

int RemoteDllLoader::CallLoudedFunctionByName(LPCSTR func_name)
{
	/*
	CallLoudedFunctionByName(LPCSTR) wont's move any parameters to remote exported function.
	*/

	IMAGE_DATA_DIRECTORY exports_data_directory =
		pe_file_->nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	PIMAGE_EXPORT_DIRECTORY current_export_location = (PIMAGE_EXPORT_DIRECTORY)
		(exports_data_directory.VirtualAddress + allocated_base_addr_);


	IMAGE_EXPORT_DIRECTORY current_export;
	ReadProcessMemory(remote_process_, (LPCVOID)(current_export_location),
		&current_export, sizeof(current_export), NULL);

	CHAR exported_module_name[MAX_PATH];
	ReadProcessMemory(remote_process_, (LPCVOID)(current_export.Name + allocated_base_addr_),
		&exported_module_name, MAX_PATH, NULL);
	LOG(std::string("Export module name : ") + std::string(exported_module_name));

	int index = -1;
	CHAR function_name[MAX_PATH]; //name of a function
	DWORD functions_names[MAX_PATH]; //array of pointers to functions names
	DWORD functions_addresses[MAX_PATH]; //array of pointers to exported functions address
	DWORD function_address; //address of a function

	ReadProcessMemory(remote_process_, (LPCVOID)(current_export.AddressOfFunctions + allocated_base_addr_),
		functions_addresses, MAX_PATH * sizeof(DWORD), NULL);
	ReadProcessMemory(remote_process_, (LPCVOID)(current_export.AddressOfNames + allocated_base_addr_),
		functions_names, MAX_PATH * sizeof(DWORD), NULL);

	for (size_t i = 0; i < current_export.NumberOfFunctions; i++)
	{
		ReadProcessMemory(remote_process_, (LPCVOID)(functions_names[i] + allocated_base_addr_),
			function_name, MAX_PATH, NULL);

		LOG(std::string(" current export function name : ") + std::string(function_name));

		if (!strcmp((const char*)function_name, func_name))
		{
			index = i;
			break;
		}
	}

	if (-1 != index)
	{
		function_address = functions_addresses[index] + allocated_base_addr_;
		if (!CreateRemoteThread(remote_process_, NULL, NULL,
			(LPTHREAD_START_ROUTINE)(function_address),	NULL, NULL, NULL))
		{
			LOG(std::string("Error calling to export func ") + std::string(function_name)
				+ std::string(" in remote process: ") + std::to_string(GetLastError()));
			return GetLastError();
		}
		return 0;
	}

	LOG(std::string("Couldn't find export func ") + std::string(function_name) + std::string(" in remote process. ") );
	return -1;
}

int RemoteDllLoader::AllocateMemory()
{
	LPVOID tmp_base_addr_ptr = VirtualAllocEx(remote_process_, &(pe_file_->nt_header.OptionalHeader.ImageBase),
		pe_file_->nt_header.OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	allocated_base_addr_ = (DWORD)tmp_base_addr_ptr;

	if (tmp_base_addr_ptr == &(pe_file_->nt_header.OptionalHeader.ImageBase))
	{
		return 0;
	}
	else
	{
		LOG("Couldn't allocate ImageBase in RemoteDllLoader::AllocateMemory : " + std::to_string(GetLastError())
			+ " trying random address");

		allocated_base_addr_ = (DWORD)VirtualAllocEx(remote_process_, NULL, pe_file_->nt_header.OptionalHeader.SizeOfImage,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!allocated_base_addr_)
		{
			LOG("Error occur in RemoteDllLoader::AllocateMemory : " + std::to_string(GetLastError()));
			return GetLastError();
		}

		LOG("succeeded allocate random address : " + std::to_string(allocated_base_addr_));
		return 0;
	}
	
	return 0;
}

int RemoteDllLoader::LoadSections()
{
	for (DWORD i = 0; i < pe_file_->number_of_sections; i++)
	{
		_IMAGE_SECTION_HEADER current_section = pe_file_->sections_headers[i];

		if (current_section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
		{
			continue;
		}

		if (!WriteProcessMemory(remote_process_, (PVOID)(allocated_base_addr_ + current_section.VirtualAddress),
			pe_file_->pe_raw + current_section.PointerToRawData, current_section.SizeOfRawData, NULL))
		{
			LOG("WriteProcessMemory failed in RemoteDllLoader::LoadSections : " + std::to_string(GetLastError()));
			return GetLastError();
		}

	}
	return 0;
}

int RemoteDllLoader::HandleRelocations()
{
	if (allocated_base_addr_ == pe_file_->nt_header.OptionalHeader.ImageBase)
	{
		//module loaded to original address space. no relocation needed.
		return 0;
	}

	DWORD delta = allocated_base_addr_ - pe_file_->nt_header.OptionalHeader.ImageBase;
	LOG("Relocation delta is : " + std::to_string(delta));

	// find relocation section
	IMAGE_SECTION_HEADER relocation_section;
	for (DWORD i = 0; i < pe_file_->number_of_sections; i++)
	{
		IMAGE_SECTION_HEADER current_section = pe_file_->sections_headers[i];
		if (strcmp((CONST PCHAR)current_section.Name, ".reloc") == 0)
		{
			relocation_section = current_section;
			break;
		}
	}

	//for each reloc block I calculate number of relocations
	PDWORD current_reloc_block = (PDWORD)
		((DWORD)pe_file_->pe_raw + (DWORD)relocation_section.PointerToRawData);
	bool is_another_reloc_block_exist = true;
	DWORD tmp_some_va = 0;
	PDWORD remote_addr_to_reloc = 0;
	DWORD remote_value_to_reloc = 0;
	int raw_data_processed = 0;
	int bytes_processed_per_round = 0;

	while (is_another_reloc_block_exist)
	{
		current_reloc_block = (DWORD*)((DWORD)bytes_processed_per_round + (DWORD)current_reloc_block);
		bytes_processed_per_round = 0;
		IMAGE_BASE_RELOCATION* current_reloc = (IMAGE_BASE_RELOCATION*)current_reloc_block;
		int relocation_count = (current_reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		bytes_processed_per_round += sizeof(IMAGE_BASE_RELOCATION);

		LOG("current reloc block RVA : " + std::to_string(current_reloc->VirtualAddress));
		LOG("current reloc block SizeOfBlock : " + std::to_string(current_reloc->SizeOfBlock));
		LOG("number of reloc : " + std::to_string(relocation_count));

		for (SIZE_T i = 0; i < relocation_count; i++)
		{
			DWORD relocation_type_offset = (DWORD)current_reloc + sizeof(IMAGE_BASE_RELOCATION) + i * sizeof(WORD);
			PWORD relocation_type_offset_fixed_size = (PWORD)relocation_type_offset;
			int reloc_type = (*relocation_type_offset_fixed_size) >> 12;

			if (reloc_type == IMAGE_REL_BASED_HIGHLOW) //Do one Relocation:
			{
				tmp_some_va = allocated_base_addr_ +
					current_reloc->VirtualAddress + ((*relocation_type_offset_fixed_size) & 4095);
				remote_addr_to_reloc = (PDWORD)tmp_some_va;

				if (!ReadProcessMemory(remote_process_, (LPCVOID)remote_addr_to_reloc,
					(LPVOID)&remote_value_to_reloc, sizeof(DWORD), NULL))
				{
					LOG("ReadProcessMemory failed in RemoteDllLoader::HandleRelocations : " + std::to_string(GetLastError()));
					return GetLastError();
				}
				remote_value_to_reloc += delta;
				if (!WriteProcessMemory(remote_process_, remote_addr_to_reloc, &remote_value_to_reloc, sizeof(DWORD), NULL))
				{
					LOG("WriteProcessMemory failed in RemoteDllLoader::HandleRelocations : " + std::to_string(GetLastError()));
					return GetLastError();
				}
				LOG(" Changed " + std::to_string((DWORD)remote_addr_to_reloc) + " from " +
					std::to_string(remote_value_to_reloc - delta) + " to " + std::to_string(remote_value_to_reloc));
			}
			bytes_processed_per_round += sizeof(WORD);
		}

		raw_data_processed += bytes_processed_per_round;
		if (raw_data_processed % sizeof(DWORD))// if alignment needed
		{
			bytes_processed_per_round += sizeof(WORD);
			raw_data_processed += sizeof(WORD);
		}
		PDWORD TestForEnd = (PDWORD)((DWORD)current_reloc_block + (DWORD)bytes_processed_per_round);
		if (!*TestForEnd)//need another check for end of reloc section if 4 bytes == 0 then end
		{
			is_another_reloc_block_exist = false;
		}
	}

	return 0;
}

int RemoteDllLoader::ResolveImports()
{
	IMAGE_DATA_DIRECTORY imports_data_directory =
		pe_file_->nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	DWORD current_remote_data_dir_location = imports_data_directory.VirtualAddress + allocated_base_addr_;

	DWORD end_of_remote_data_directory =
		allocated_base_addr_ + imports_data_directory.VirtualAddress + imports_data_directory.Size;

	//need to get current_import from the other process
	IMAGE_IMPORT_DESCRIPTOR current_import;
	DWORD current_import_address = current_remote_data_dir_location;
	ReadProcessMemory(remote_process_, (LPCVOID)current_remote_data_dir_location, &current_import, sizeof(current_import), NULL);
	
	CHAR imported_module_name[MAX_PATH]; //name of each module
	DWORD function_names[MAX_PATH]; //array of pointers to IMAGE_IMPORT_BY_NAME
	IMAGE_IMPORT_BY_NAME imported_function_entry;
	CHAR function_name[MAX_PATH]; //name of a function
	DWORD functions_IAT[MAX_PATH]; //array of pointers to imports function
	DWORD function_address; //address of imported function

	while (current_import_address < end_of_remote_data_directory)
	{
		if (!current_import.Characteristics) //check for the end of IMAGE_IMPORT_DESCRIPTOR array
		{
			break;
		}
		ReadProcessMemory(remote_process_, (LPCVOID)(current_import.Name + allocated_base_addr_),
			imported_module_name, MAX_PATH, NULL);

		LOG(std::string("current import module name : ") + std::string(imported_module_name));
		
		ReadProcessMemory(remote_process_, (LPCVOID)(current_import.Characteristics + allocated_base_addr_),
			function_names, MAX_PATH*sizeof(DWORD), NULL);
		
		ReadProcessMemory(remote_process_, (LPCVOID)(current_import.FirstThunk + allocated_base_addr_),
			functions_IAT, MAX_PATH * sizeof(DWORD), NULL);

		HMODULE remote_module = GetRemoteModuleHandle(remote_process_, imported_module_name);
		if (!remote_module)
		{
			LOG(std::string(" current import not loaded on remote process, loading it"));
			remote_module = LoadModuleRemoteProcess(remote_process_, imported_module_name);
			if (!remote_module)
			{
				LOG(std::string("  failed loading ") + std::string(imported_module_name) 
					+ std::string(" to remote process. Aborting."));
				return NULL;
			}
		}

		WORD hint = !NULL;
		int i = 0;
		for (; hint; i++)
		{
			ReadProcessMemory(remote_process_, (LPCVOID)(function_names[i] + allocated_base_addr_),
				&imported_function_entry, sizeof(IMAGE_IMPORT_BY_NAME), NULL);

			ReadProcessMemory(remote_process_, 
				(LPCVOID)(function_names[i] + allocated_base_addr_ +sizeof(imported_function_entry.Hint)),
				function_name, MAX_PATH, NULL);

			WORD hint = imported_function_entry.Hint;

			if (!hint)
			{
				//end of this module
				break;
			}

			functions_IAT[i] = (DWORD)GetRemoteProcAddress(remote_process_, remote_module, function_name, NULL, FALSE);
#ifdef DEBUG
			if (functions_IAT[i])
			{
				LOG(std::string(" current import func name :") + std::to_string(hint) + " : " + std::string(function_name)
					+ " loaded success " + std::to_string(functions_IAT[i]));
			}
			else
			{
				LOG(std::string(" current import func name :") + std::to_string(hint) + " : " + std::string(function_name)
					+ " loaded FAILED ");
			}
#endif // DEBUG
		}
		
		if (!WriteProcessMemory(remote_process_, (LPVOID)(current_import.FirstThunk + allocated_base_addr_),
			functions_IAT, i * sizeof(DWORD), NULL))
		{
			LOG("WriteProcessMemory failed in RemoteDllLoader::ResolveImports : " + std::to_string(GetLastError()));
			return GetLastError();
		}
		current_import_address = current_import_address + sizeof(IMAGE_IMPORT_DESCRIPTOR);
		ReadProcessMemory(remote_process_, (LPCVOID)current_import_address, &current_import, sizeof(current_import), NULL);
	}

	return 0;
}

int RemoteDllLoader::ProtectMemory()
{
	
	PDWORD old_protect = new DWORD;
	for (int i = 0; i < pe_file_->number_of_sections; i++)
	{
		_IMAGE_SECTION_HEADER current_section = pe_file_->sections_headers[i];
		if (current_section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
		{
			continue;
		}

		if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ
			&& current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtectEx(remote_process_, (PVOID)((allocated_base_addr_)+current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_READWRITE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_WRITE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtectEx(remote_process_, (PVOID)((allocated_base_addr_)+current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_READWRITE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtectEx(remote_process_, (PVOID)((allocated_base_addr_)+current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_READ, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtectEx(remote_process_, (PVOID)((allocated_base_addr_)+current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_WRITECOPY, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE)
		{
			VirtualProtectEx(remote_process_, (PVOID)((allocated_base_addr_)+current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtectEx(remote_process_, (PVOID)((allocated_base_addr_)+current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_READONLY, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtectEx(remote_process_, (PVOID)((allocated_base_addr_)+current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_WRITECOPY, old_protect);
		}

	}
	delete old_protect;
	return 0;
}
