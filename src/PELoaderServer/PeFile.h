#pragma once

#include <Windows.h>


struct PeFile {
	IMAGE_DOS_HEADER dos_header;
	IMAGE_NT_HEADERS32 nt_header;
	int number_of_sections;
	PIMAGE_SECTION_HEADER sections_headers;
	byte* pe_raw;
};
