
#include "Loader.h"
#include "Debug.h"

Loader::Loader(PeFile* pe_obj)
{
	LOG("Loader created");
	m_allocated_base_address = NULL;
	m_pe_file = pe_obj;
}

int Loader::load()
{
	allocateMemory();
	loadSections();
	handleRelocations();
	resolveImports();
	protectMemory();
	return 0;
}

int Loader::attach() const
{
	const auto entry = reinterpret_cast<DllEntryProc>(m_allocated_base_address + m_pe_file->nt_header.OptionalHeader.AddressOfEntryPoint);
	(*entry)(reinterpret_cast<HINSTANCE>(m_allocated_base_address), DLL_PROCESS_ATTACH, 0);
	return 0;
}

PDWORD Loader::getLoadedFunctionByName(char* funcName) const
{
	const IMAGE_DATA_DIRECTORY exportsDataDirectory =
		m_pe_file->nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	const auto currentExport = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(exportsDataDirectory.VirtualAddress + m_allocated_base_address);
	const auto exportedModuleName = reinterpret_cast<char*>(currentExport->Name + m_allocated_base_address);

	LOG(std::string("current export module name : ") + std::string(exportedModuleName));

	const auto functionsNamesAddresses = reinterpret_cast<char**>(currentExport->AddressOfNames + m_allocated_base_address);
	auto index = -1;
	for (size_t i = 0; i < currentExport->NumberOfFunctions; i++)
	{
		auto currentFunctionName = functionsNamesAddresses[i] + m_allocated_base_address;

		LOG(std::string(" current export func name : ") + std::string(currentFunctionName));

		if (!strcmp(static_cast<const char*>(currentFunctionName), funcName))
		{
			index = i;
			break;
		}
	}
	if (-1 == index)
	{
		return nullptr;
	}

	const auto functionsAddresses = reinterpret_cast<PDWORD>(currentExport->AddressOfFunctions + m_allocated_base_address);
	const auto currentFunctionAddress = functionsAddresses[index] + m_allocated_base_address;

	return reinterpret_cast<PDWORD>(currentFunctionAddress);;
}

int Loader::allocateMemory()
{
	LPVOID tmp_base_addr_ptr = VirtualAlloc(&(m_pe_file->nt_header.OptionalHeader.ImageBase),
		m_pe_file->nt_header.OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	m_allocated_base_address = (DWORD)tmp_base_addr_ptr;

	if (tmp_base_addr_ptr == &(m_pe_file->nt_header.OptionalHeader.ImageBase))
	{
		return 0;
	}
	else
	{
		LOG("Couldn't allocate ImageBase in Loader::AllocateMemory : " + std::to_string(GetLastError())
		+ " trying random address");

		m_allocated_base_address = (DWORD)VirtualAlloc(NULL, m_pe_file->nt_header.OptionalHeader.SizeOfImage,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!m_allocated_base_address)
		{
			LOG("Error occur in Loader::AllocateMemory : " + std::to_string(GetLastError()));
			return GetLastError();
		}

		LOG("succeeded allocate random address : " + std::to_string(m_allocated_base_address));
		return 0;
	}
}

int Loader::loadSections()
{
	for (int i = 0; i < m_pe_file->number_of_sections; i++)
	{
		_IMAGE_SECTION_HEADER current_section = m_pe_file->sections_headers[i];

		if (current_section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
		{
			continue;
		}

		memcpy((void*)(m_allocated_base_address + current_section.VirtualAddress),
			m_pe_file->pe_raw + current_section.PointerToRawData,
			current_section.SizeOfRawData);
	}
	return 0;
}

int Loader::handleRelocations()
{
	if (m_allocated_base_address == m_pe_file->nt_header.OptionalHeader.ImageBase)
	{
		//module loaded to original address space. no relocation needed.
		return 0;
	}

	DWORD delta = m_allocated_base_address - m_pe_file->nt_header.OptionalHeader.ImageBase;
	LOG("Relocation delta is : " + std::to_string(delta));

	for (int i = 0; i < m_pe_file->number_of_sections; i++)
	{
		IMAGE_SECTION_HEADER current_section = m_pe_file->sections_headers[i];
		if (strcmp((const char*)current_section.Name, ".reloc") != 0)
		{
			continue;
		}

		//for each reloc block I calculate number of relocations
		PDWORD current_reloc_block = (PDWORD)
			((DWORD)m_pe_file->pe_raw + (DWORD)current_section.PointerToRawData);
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
					tmp_some_va = m_allocated_base_address +
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

int Loader::resolveImports()
{
	IMAGE_DATA_DIRECTORY imports_data_directory =
		m_pe_file->nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	DWORD current_data_dir_location = imports_data_directory.VirtualAddress + m_allocated_base_address;

	DWORD end_of_data_directory =
		m_allocated_base_address + imports_data_directory.VirtualAddress + imports_data_directory.Size;

	PIMAGE_IMPORT_DESCRIPTOR current_import = (PIMAGE_IMPORT_DESCRIPTOR)current_data_dir_location;
	while (current_data_dir_location < end_of_data_directory)
	{
		if (!current_import->Characteristics) //check for the end of IMAGE_IMPORT_DESCRIPTOR array
		{
			break;
		}
		char* imported_module_name = (char*)(current_import->Name + m_allocated_base_address);
		LOG(std::string("current import module name : ") + std::string(imported_module_name));

		PDWORD hint_name_arr = (PDWORD)(current_import->Characteristics + m_allocated_base_address);
		for (int i = 0; hint_name_arr; hint_name_arr++, i++)
		{
			DWORD current_function_name = ((*hint_name_arr) + m_allocated_base_address);
			WORD hint = (WORD)(*(PDWORD)current_function_name);
			char* func_name = (char*)(PDWORD)(current_function_name + sizeof(WORD));

			if (!hint)
			{
				//end of this module
				break;
			}

			DWORD current_loaded_func_add = current_import->FirstThunk + i * sizeof(current_import->FirstThunk) 
				+ m_allocated_base_address;
			*(PDWORD)current_loaded_func_add = getImportedFunctionAddress(imported_module_name, func_name);
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

int Loader::protectMemory()
{
	PDWORD old_protect = new DWORD;
	for (int i = 0; i < m_pe_file->number_of_sections; i++)
	{
		_IMAGE_SECTION_HEADER current_section = m_pe_file->sections_headers[i];
		if (current_section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
		{
			continue;
		}

		if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ
			&& current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect((void*)((m_allocated_base_address) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_READWRITE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_WRITE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect((void*)((m_allocated_base_address) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_READWRITE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect((void*)((m_allocated_base_address) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_READ, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect((void*)((m_allocated_base_address) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE_WRITECOPY, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_EXECUTE)
		{
			VirtualProtect((void*)((m_allocated_base_address) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_EXECUTE, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect((void*)((m_allocated_base_address) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_READONLY, old_protect);
		}

		else if (current_section.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect((void*)((m_allocated_base_address) + current_section.VirtualAddress),
				current_section.SizeOfRawData, PAGE_WRITECOPY, old_protect);
		}

	}
	return 0;
}

DWORD Loader::getImportedFunctionAddress(char* module_name, char* func_name)
{
	HINSTANCE get_proc_id_dll = LoadLibraryA(module_name);
	if (!get_proc_id_dll)
	{
		return NULL;
	}
	return (DWORD)GetProcAddress(get_proc_id_dll, func_name);
}
