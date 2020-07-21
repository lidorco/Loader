#pragma once

#include "Seh.h"
#include "Debug.h"


int getAmountOfSehHandler(PeFile& peFile, DWORD allocatedBaseAddress)
{
	const auto configDataDirectory = peFile.nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
	const auto currentConfigDataDirectory = configDataDirectory.VirtualAddress + allocatedBaseAddress;
	auto configDirectory = reinterpret_cast<PIMAGE_LOAD_CONFIG_DIRECTORY32>(currentConfigDataDirectory);
	return configDirectory->SEHandlerCount;
}

int isSehUsed(PeFile& peFile, DWORD allocatedBaseAddress)
{
	// Check for SEH in 32 bit PE:
	if (peFile.nt_header.OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NO_SEH)
	{
		LOG(std::string(" IMAGE_DLLCHARACTERISTICS_NO_SEH is set"));
		return 0;
	}
	else
	{
		LOG(std::string(" IMAGE_DLLCHARACTERISTICS_NO_SEH is NOT set"));
	}

	// Check SEH table size:
	const auto configDataDirectory = peFile.nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
	const auto currentConfigDataDirectory = configDataDirectory.VirtualAddress + allocatedBaseAddress;
	auto configDirectory = reinterpret_cast<PIMAGE_LOAD_CONFIG_DIRECTORY32>(currentConfigDataDirectory);
	LOG(std::string(" configDirectory->SEHandlerCount :") + std::to_string(configDirectory->SEHandlerCount));
	LOG(std::string(" configDirectory->SEHandlerTable :") + std::to_string(configDirectory->SEHandlerTable));

	if (configDirectory->SEHandlerCount == 0)
	{
		return 0;
	}

	// Parse seh table:
	auto currentHandlerAddress = configDirectory->SEHandlerTable; // relocation fix this
	for (int i = 0; i < configDirectory->SEHandlerCount; i++)
	{
		auto handler = *reinterpret_cast<PDWORD>(currentHandlerAddress) + allocatedBaseAddress;

		LOG(std::string(" hanlder ") + std::to_string(i) + " handler code at " + to_hexstring(handler));
		currentHandlerAddress += sizeof(PEXCEPTION_REGISTRATION_RECORD);
	}

	return 1;
}


void printSehChain()
{
	// view all seh handler:
	PEXCEPTION_REGISTRATION_RECORD lastSehHandler = nullptr;
	__asm {
		mov eax, FS: [0] ;
		mov lastSehHandler, eax;
	}

	auto currentHandler = lastSehHandler;
	while (currentHandler != 0 && reinterpret_cast<DWORD>(currentHandler) != 0xffffffff)
	{
		LOG(std::string(" next ") + to_hexstring(reinterpret_cast<DWORD>(currentHandler->Next)) +
			" handler code at " + to_hexstring(reinterpret_cast<DWORD>(currentHandler->Handler)));

		currentHandler = currentHandler->Next;
	}
}

PEXCEPTION_REGISTRATION_RECORD getLastSehHandler()
{
	PEXCEPTION_REGISTRATION_RECORD lastSehHandler = nullptr;
	__asm {
		mov eax, FS: [0] ;
		mov lastSehHandler, eax;
	}
	return lastSehHandler;
}

void addToSehChain(void* where, PeFile& peFile, DWORD allocatedBaseAddress)
{
	printSehChain();
	PEXCEPTION_REGISTRATION_RECORD lastSehHandler = getLastSehHandler();

	// add handler to !exchain:
	const auto configDataDirectory = peFile.nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
	const auto currentConfigDataDirectory = configDataDirectory.VirtualAddress + allocatedBaseAddress;
	auto configDirectory = reinterpret_cast<PIMAGE_LOAD_CONFIG_DIRECTORY32>(currentConfigDataDirectory);

	auto currentHandlerAddress = configDirectory->SEHandlerTable; // relocation fix this
	PEXCEPTION_REGISTRATION_RECORD record = nullptr;
	for (int i = 0; i < configDirectory->SEHandlerCount; i++)
	{
		auto handler = *reinterpret_cast<PDWORD>(currentHandlerAddress) + allocatedBaseAddress;

		record = reinterpret_cast<PEXCEPTION_REGISTRATION_RECORD>(where) +
			(configDirectory->SEHandlerCount - (i + 1)) * sizeof(EXCEPTION_REGISTRATION_RECORD);

		record->Next = lastSehHandler;
		record->Handler = reinterpret_cast<PEXCEPTION_ROUTINE>(handler);

		lastSehHandler = record;
		LOG(std::string(" hanlder ") + std::to_string(i) + " handler code at " + std::to_string(handler));
		currentHandlerAddress += sizeof(PEXCEPTION_REGISTRATION_RECORD);
	}

	//set the last exception
	__asm {
		mov eax, record;
		mov FS : [0] , eax;
	}
}



BOOL WINAPI MyRtlIsValidHandler(DWORD handler)
{
	LOG(std::string(" MyRtlIsValidHandler called ") + to_hexstring(handler) + " at thread " + to_hexstring(GetThreadId(GetCurrentThread())));
	return FALSE;
}

__declspec(naked) void  detour()
{
	__asm {
		mov eax, MyRtlIsValidHandler;
		jmp eax;
	}
}
int detourSize = 7;


void hookRtlIsValidHandler()
{
	const DWORD RtlIsValidHandlerOffset = 0x63cd8;
	auto ntdll = LoadLibraryA("C:\\Windows\\SysWOW64\\ntdll.dll");
	PDWORD RtlIsValidHandlerAddress = reinterpret_cast<PDWORD>(GetProcAddress(ntdll, "RtlIsValidHandler"));
	if (RtlIsValidHandlerAddress == 0)
	{
		RtlIsValidHandlerAddress = reinterpret_cast<PDWORD>(reinterpret_cast<DWORD>(ntdll) + RtlIsValidHandlerOffset);
	}

	DWORD oldProtect = 0;
	auto result = VirtualProtect(RtlIsValidHandlerAddress, detourSize, PAGE_EXECUTE_READWRITE, &oldProtect);

	unsigned char* detourCodeAddress = reinterpret_cast<unsigned char*>(detour); // need to make sure Link Incrementally is disable - /INCREMENTAL[:NO]
	memcpy(reinterpret_cast<unsigned char*>(RtlIsValidHandlerAddress), detourCodeAddress, detourSize);

	DWORD tmp = 0;
	result = VirtualProtect(RtlIsValidHandlerAddress, detourSize, oldProtect, &tmp);
}
