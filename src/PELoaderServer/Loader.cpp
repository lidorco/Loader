
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
	LPVOID tmpBaseAddressPtr = VirtualAlloc(&(m_pe_file->nt_header.OptionalHeader.ImageBase),
		m_pe_file->nt_header.OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	m_allocated_base_address = reinterpret_cast<DWORD>(tmpBaseAddressPtr);

	if (tmpBaseAddressPtr == &(m_pe_file->nt_header.OptionalHeader.ImageBase))
	{
		return 0;
	}
	else
	{
		LOG("Couldn't allocate ImageBase in Loader::AllocateMemory : " + std::to_string(GetLastError())
		+ " trying random address");

		m_allocated_base_address = reinterpret_cast<DWORD>(VirtualAlloc(nullptr, m_pe_file->nt_header.OptionalHeader.SizeOfImage,
		                                                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		if (!m_allocated_base_address)
		{
			LOG("Error occur in Loader::AllocateMemory : " + std::to_string(GetLastError()));
			return GetLastError();
		}

		LOG("succeeded allocate random address : " + std::to_string(m_allocated_base_address));
		return 0;
	}
}

int Loader::loadSections() const
{
	for (auto i = 0; i < m_pe_file->number_of_sections; i++)
	{
		const auto currentSection = m_pe_file->sections_headers[i];

		if (currentSection.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
		{
			continue;
		}

		memcpy(reinterpret_cast<void*>(m_allocated_base_address + currentSection.VirtualAddress),
			m_pe_file->pe_raw + currentSection.PointerToRawData,
			currentSection.SizeOfRawData);
	}
	return 0;
}

int Loader::handleRelocations() const
{
	if (m_allocated_base_address == m_pe_file->nt_header.OptionalHeader.ImageBase)
	{
		//module loaded to original address space. no relocation needed.
		return 0;
	}

	const auto delta = m_allocated_base_address - m_pe_file->nt_header.OptionalHeader.ImageBase;
	LOG("Relocation delta is : " + std::to_string(delta));

	for (auto i = 0; i < m_pe_file->number_of_sections; i++)
	{
		const auto currentSection = m_pe_file->sections_headers[i];
		if (strcmp(reinterpret_cast<const char*>(currentSection.Name), ".reloc") != 0)
		{
			continue;
		}

		//for each reloc block I calculate number of relocations
		auto currentRelocBlock = reinterpret_cast<PDWORD>(reinterpret_cast<DWORD>(m_pe_file->pe_raw) + static_cast<DWORD>(currentSection.PointerToRawData));
		auto isAnotherRelocBlockExist = true;

		DWORD tmpSomeVa;
		DWORD* tmpSomeVaPtr;
		int rawDataProcessed = 0;
		int rawDataProcessedPerRound = 0;
		while (isAnotherRelocBlockExist)
		{
			currentRelocBlock = reinterpret_cast<DWORD*>(static_cast<DWORD>(rawDataProcessedPerRound) + reinterpret_cast<DWORD>(currentRelocBlock));
			rawDataProcessedPerRound = 0;
			auto* currentReloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(currentRelocBlock);
			const int relocationCount = (currentReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			rawDataProcessedPerRound += sizeof(IMAGE_BASE_RELOCATION);

			LOG("current reloc block RVA : " + std::to_string(currentReloc->VirtualAddress));
			LOG("current reloc block SizeOfBlock : " + std::to_string(currentReloc->SizeOfBlock));
			LOG("number of reloc : " + std::to_string(relocationCount));

			for (size_t i = 0; i < relocationCount; i++)
			{
				const auto typeOffset = reinterpret_cast<DWORD>(currentReloc) + i * sizeof(WORD) + sizeof(IMAGE_BASE_RELOCATION);
				const auto fixedSizeTypeOffset = reinterpret_cast<WORD*>(typeOffset);
				const int relocType = (*fixedSizeTypeOffset) >> 12;
				if (relocType == IMAGE_REL_BASED_HIGHLOW)
				{
					tmpSomeVa = m_allocated_base_address +
						currentReloc->VirtualAddress + ((*fixedSizeTypeOffset) & 4095);

					tmpSomeVaPtr = reinterpret_cast<DWORD*>(tmpSomeVa);

					*tmpSomeVaPtr += delta;
				}
				rawDataProcessedPerRound += sizeof(WORD);

			}

			rawDataProcessed += rawDataProcessedPerRound;
			if (rawDataProcessed % sizeof(DWORD))
			{
				rawDataProcessedPerRound += sizeof(WORD);
				rawDataProcessed += sizeof(WORD);
			}
			const auto testForEnd = reinterpret_cast<DWORD*>(reinterpret_cast<DWORD>(currentRelocBlock) + static_cast<DWORD>(rawDataProcessedPerRound));
			if (!*testForEnd)//need another check for end of reloc sec if 4 bytes == 0 then end
			{
				isAnotherRelocBlockExist = false;
			}
		}


	}
	return 0;
}

int Loader::resolveImports()
{
	const auto importsDataDirectory =
		m_pe_file->nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	const auto currentDataDirLocation = importsDataDirectory.VirtualAddress + m_allocated_base_address;

	const auto endOfDataDirectory =
		m_allocated_base_address + importsDataDirectory.VirtualAddress + importsDataDirectory.Size;

	auto currentImport = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(currentDataDirLocation);
	while (currentDataDirLocation < endOfDataDirectory)
	{
		if (!currentImport->Characteristics) //check for the end of IMAGE_IMPORT_DESCRIPTOR array
		{
			break;
		}
		auto imported_module_name = reinterpret_cast<char*>(currentImport->Name + m_allocated_base_address);
		LOG(std::string("current import module name : ") + std::string(imported_module_name));

		auto hint_name_arr = reinterpret_cast<PDWORD>(currentImport->Characteristics + m_allocated_base_address);
		for (int i = 0; hint_name_arr; hint_name_arr++, i++)
		{
			const auto currentFunctionName = ((*hint_name_arr) + m_allocated_base_address);
			const auto hint = static_cast<WORD>(*reinterpret_cast<PDWORD>(currentFunctionName));
			const auto functionName = reinterpret_cast<char*>(reinterpret_cast<PDWORD>(currentFunctionName + sizeof(WORD)));

			if (!hint)
			{
				//end of this module
				break;
			}

			const auto currentLoadedFunctionAddress = currentImport->FirstThunk + i * sizeof(currentImport->FirstThunk) 
				+ m_allocated_base_address;
			*reinterpret_cast<PDWORD>(currentLoadedFunctionAddress) = getImportedFunctionAddress(imported_module_name, functionName);
#ifdef DEBUG
			if (*reinterpret_cast<PDWORD>(currentLoadedFunctionAddress))
			{
				LOG(std::string(" current import func name :") + std::to_string(hint) + " : " + std::string(functionName)
				 + " loaded success " + std::to_string(*reinterpret_cast<PDWORD>(currentLoadedFunctionAddress)));
			}
			else
			{
				LOG(std::string(" current import func name :") + std::to_string(hint) + " : " + std::string(functionName)
					+ " loaded FAILED ");
			}
#endif // DEBUG
		}

		currentImport = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(reinterpret_cast<DWORD>(currentImport) + sizeof(IMAGE_IMPORT_DESCRIPTOR));
	}

	return 0;
}

int Loader::protectMemory() const
{
	const auto oldProtect = new DWORD;
	for (auto i = 0; i < m_pe_file->number_of_sections; i++)
	{
		const auto currentSection = m_pe_file->sections_headers[i];
		if (currentSection.Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
		{
			continue;
		}

		if (currentSection.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& currentSection.Characteristics & IMAGE_SCN_MEM_READ
			&& currentSection.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect(reinterpret_cast<void*>((m_allocated_base_address) + currentSection.VirtualAddress),
				currentSection.SizeOfRawData, PAGE_EXECUTE_READWRITE, oldProtect);
		}

		else if (currentSection.Characteristics & IMAGE_SCN_MEM_WRITE
			&& currentSection.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect(reinterpret_cast<void*>((m_allocated_base_address) + currentSection.VirtualAddress),
				currentSection.SizeOfRawData, PAGE_READWRITE, oldProtect);
		}

		else if (currentSection.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& currentSection.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect(reinterpret_cast<void*>((m_allocated_base_address) + currentSection.VirtualAddress),
				currentSection.SizeOfRawData, PAGE_EXECUTE_READ, oldProtect);
		}

		else if (currentSection.Characteristics & IMAGE_SCN_MEM_EXECUTE
			&& currentSection.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect(reinterpret_cast<void*>((m_allocated_base_address) + currentSection.VirtualAddress),
				currentSection.SizeOfRawData, PAGE_EXECUTE_WRITECOPY, oldProtect);
		}

		else if (currentSection.Characteristics & IMAGE_SCN_MEM_EXECUTE)
		{
			VirtualProtect(reinterpret_cast<void*>((m_allocated_base_address) + currentSection.VirtualAddress),
				currentSection.SizeOfRawData, PAGE_EXECUTE, oldProtect);
		}

		else if (currentSection.Characteristics & IMAGE_SCN_MEM_READ)
		{
			VirtualProtect(reinterpret_cast<void*>((m_allocated_base_address) + currentSection.VirtualAddress),
				currentSection.SizeOfRawData, PAGE_READONLY, oldProtect);
		}

		else if (currentSection.Characteristics & IMAGE_SCN_MEM_WRITE)
		{
			VirtualProtect(reinterpret_cast<void*>((m_allocated_base_address) + currentSection.VirtualAddress),
				currentSection.SizeOfRawData, PAGE_WRITECOPY, oldProtect);
		}

	}
	return 0;
}

DWORD Loader::getImportedFunctionAddress(char* moduleName, char* functionName)
{
	const auto getProcIdDll = LoadLibraryA(moduleName);
	if (!getProcIdDll)
	{
		return NULL;
	}
	return reinterpret_cast<DWORD>(GetProcAddress(getProcIdDll, functionName));
}
