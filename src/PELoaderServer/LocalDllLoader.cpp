
#include "LocalDllLoader.h"
#include "Debug.h"

LocalDllLoader::LocalDllLoader(PeFile* pe_obj)
{
	LOG("LocalDllLoader created");
	allocated_base_addr_ = NULL;
	pe_file_ = pe_obj;
}

int LocalDllLoader::Load()
{
	AllocateMemory();
	LoadSections();
	HandleRelocations();
	ResolveImports();
	ProtectMemory();
	return 0;
}

int LocalDllLoader::Attach()
{
	DllEntryProc entry = (DllEntryProc)(allocated_base_addr_ + pe_file_->nt_header.OptionalHeader.AddressOfEntryPoint);
	(*entry)((HINSTANCE)allocated_base_addr_, DLL_PROCESS_ATTACH, 0);
	return 0;
}

PDWORD LocalDllLoader::GetLoudedFunctionByName(char* func_name)
{
	IMAGE_DATA_DIRECTORY exports_data_directory =
		pe_file_->nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	PIMAGE_EXPORT_DIRECTORY current_export = (PIMAGE_EXPORT_DIRECTORY)
		(exports_data_directory.VirtualAddress + allocated_base_addr_);
	char* exported_module_name = (char*)(current_export->Name + allocated_base_addr_);

	LOG(std::string("current export module name : ") + std::string(exported_module_name));

	char** functions_names_addresses = (char**)(current_export->AddressOfNames + allocated_base_addr_);
	int index = -1;
	for (size_t i = 0; i < current_export->NumberOfFunctions; i++)
	{
		char* current_function_name = functions_names_addresses[i] + allocated_base_addr_;

		LOG(std::string(" current export func name : ") + std::string(current_function_name));

		if (!strcmp((const char*)current_function_name, func_name))
		{
			index = i;
			break;
		}
	}
	if (-1 == index)
	{
		return NULL;
	}

	PDWORD functions_addresses = (PDWORD)(current_export->AddressOfFunctions + allocated_base_addr_);
	DWORD current_function_address = functions_addresses[index] + allocated_base_addr_;

	return (PDWORD)current_function_address;;
}

int LocalDllLoader::AllocateMemory()
{
	LPVOID tmp_base_addr_ptr = VirtualAlloc(&(pe_file_->nt_header.OptionalHeader.ImageBase),
		pe_file_->nt_header.OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	allocated_base_addr_ = (DWORD)tmp_base_addr_ptr;

	if (tmp_base_addr_ptr == &(pe_file_->nt_header.OptionalHeader.ImageBase))
	{
		return 0;
	}
	else
	{
		LOG("Couldn't allocate ImageBase in LocalDllLoader::AllocateMemory : " + std::to_string(GetLastError())
		+ " trying random address");

		allocated_base_addr_ = (DWORD)VirtualAlloc(NULL, pe_file_->nt_header.OptionalHeader.SizeOfImage,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!allocated_base_addr_)
		{
			LOG("Error occur in LocalDllLoader::AllocateMemory : " + std::to_string(GetLastError()));
			return GetLastError();
		}

		LOG("succeeded allocate random address : " + std::to_string(allocated_base_addr_));
		return 0;
	}
}

int LocalDllLoader::LoadSections()
{
	for (int i = 0; i < pe_file_->number_of_sections; i++)
	{
		_IMAGE_SECTION_HEADER current_section = pe_file_->sections_headers[i];

		if (current_section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
		{
			continue;
		}

		CopyMemory((PVOID)(allocated_base_addr_ + current_section.VirtualAddress),
			pe_file_->pe_raw + current_section.PointerToRawData,
			current_section.SizeOfRawData);
	}
	return 0;
}

int LocalDllLoader::HandleRelocations()
{
	if (allocated_base_addr_ == pe_file_->nt_header.OptionalHeader.ImageBase)
	{
		//module loaded to original address space. no relocation needed.
		return 0;
	}

	DWORD delta = allocated_base_addr_ - pe_file_->nt_header.OptionalHeader.ImageBase;
	LOG("Relocation delta is : " + std::to_string(delta));

	for (int i = 0; i < pe_file_->number_of_sections; i++) // find relocation section
	{
		IMAGE_SECTION_HEADER current_section = pe_file_->sections_headers[i];
		if (strcmp((const char*)current_section.Name, ".reloc") != 0)
		{
			continue;
		}

		//for each reloc block I calculate number of relocations
		PDWORD current_reloc_block = (PDWORD)
			((DWORD)pe_file_->pe_raw + (DWORD)current_section.PointerToRawData);
		bool is_another_reloc_block_exist = true;

		DWORD tmp_some_va;
		DWORD* tmp_some_va_ptr;
		int raw_data_processed = 0;
		int raw_data_processed_per_round = 0;
		while (is_another_reloc_block_exist)
		{
			current_reloc_block = (DWORD*)((DWORD)raw_data_processed_per_round + (DWORD)current_reloc_block);
			raw_data_processed_per_round = 0;
			IMAGE_BASE_RELOCATION* current_reloc = (IMAGE_BASE_RELOCATION*)current_reloc_block;
			int relocationCount = (current_reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			raw_data_processed_per_round += sizeof(IMAGE_BASE_RELOCATION);

			LOG("current reloc block RVA : " + std::to_string(current_reloc->VirtualAddress));
			LOG("current reloc block SizeOfBlock : " + std::to_string(current_reloc->SizeOfBlock));
			LOG("number of reloc : " + std::to_string(relocationCount));

			for (size_t i = 0; i < relocationCount; i++)
			{
				DWORD TypeOffset = (DWORD)current_reloc + i * sizeof(WORD) + sizeof(IMAGE_BASE_RELOCATION);
				WORD* fixedSizeTypeOffset = (WORD*)TypeOffset;
				int reloc_type = (*fixedSizeTypeOffset) >> 12;
				if (reloc_type == IMAGE_REL_BASED_HIGHLOW)
				{
					tmp_some_va = allocated_base_addr_ +
						current_reloc->VirtualAddress + ((*fixedSizeTypeOffset) & 4095);

					tmp_some_va_ptr = (DWORD*)tmp_some_va;

					*tmp_some_va_ptr += delta;
				}
				raw_data_processed_per_round += sizeof(WORD);

			}

			raw_data_processed += raw_data_processed_per_round;
			if (raw_data_processed % sizeof(DWORD))
			{
				raw_data_processed_per_round += sizeof(WORD);
				raw_data_processed += sizeof(WORD);
			}
			DWORD* TestForEnd = (DWORD*)((DWORD)current_reloc_block + (DWORD)raw_data_processed_per_round);
			if (!*TestForEnd)//need another check for end of reloc sec if 4 bytes == 0 then end
			{
				is_another_reloc_block_exist = false;
			}
		}


	}
	return 0;
}

int LocalDllLoader::ResolveImports()
{
	IMAGE_DATA_DIRECTORY imports_data_directory =
		pe_file_->nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	DWORD current_data_dir_location = imports_data_directory.VirtualAddress + allocated_base_addr_;

	DWORD end_of_data_directory =
		allocated_base_addr_ + imports_data_directory.VirtualAddress + imports_data_directory.Size;

	PIMAGE_IMPORT_DESCRIPTOR current_import = (PIMAGE_IMPORT_DESCRIPTOR)current_data_dir_location;
	while (current_data_dir_location < end_of_data_directory)
	{
		if (!current_import->Characteristics) //check for the end of IMAGE_IMPORT_DESCRIPTOR array
		{
			break;
		}
		char* imported_module_name = (char*)(current_import->Name + allocated_base_addr_);
		LOG(std::string("current import module name : ") + std::string(imported_module_name));

		PDWORD hint_name_arr = (PDWORD)(current_import->Characteristics + allocated_base_addr_);
		for (int i = 0; hint_name_arr; hint_name_arr++, i++)
		{
			DWORD current_function_name = ((*hint_name_arr) + allocated_base_addr_);
			WORD hint = (WORD)(*(PDWORD)current_function_name);
			char* func_name = (char*)(PDWORD)(current_function_name + sizeof(WORD));

			if (!hint)
			{
				//end of this module
				break;
			}

			DWORD current_loaded_func_add = current_import->FirstThunk + i * sizeof(current_import->FirstThunk) 
				+ allocated_base_addr_;
			*(PDWORD)current_loaded_func_add = GetImportedFunctionAddress(imported_module_name, func_name);
#ifdef DEBUG
			if (*(PDWORD)current_loaded_func_add)
			{
				LOG(std::string(" current import func name :") + std::to_string(hint) + " : " + std::string(func_name)
				 + " loaded success " + std::to_string(*(PDWORD)current_loaded_func_add));
			}
			else
			{
				LOG(std::string(" current import func name :") + std::to_string(hint) + " : " + std::string(func_name)
					+ " loaded FAILED ");
			}
#endif // DEBUG
		}

		current_import = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)current_import + sizeof(IMAGE_IMPORT_DESCRIPTOR));
	}

	return 0;
}

int LocalDllLoader::ProtectMemory()
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
			VirtualProtect((void*)((allocated_base_addr_) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_READWRITE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_WRITE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect((void*)((allocated_base_addr_) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_READWRITE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect((void*)((allocated_base_addr_) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_READ, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect((void*)((allocated_base_addr_) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_WRITECOPY, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE)
		{
			VirtualProtect((void*)((allocated_base_addr_) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect((void*)((allocated_base_addr_) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_READONLY, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect((void*)((allocated_base_addr_) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_WRITECOPY, old_protect);
		}

	}
	return 0;
}

DWORD LocalDllLoader::GetImportedFunctionAddress(char* module_name, char* func_name)
{
	HINSTANCE get_proc_id_dll = LoadLibraryA(module_name);
	if (!get_proc_id_dll)
	{
		return NULL;
	}
	return (DWORD)GetProcAddress(get_proc_id_dll, func_name);
}
