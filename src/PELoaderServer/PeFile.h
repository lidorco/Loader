#pragma once

#ifndef __WINDOWS_H_INCLUDED__
#define __WINDOWS_H_INCLUDED__
#include <windows.h>
#endif // __WINDOWS_H_INCLUDED__ 


struct PeFile {
	IMAGE_DOS_HEADER dos_header;
	IMAGE_NT_HEADERS32 nt_header;
	int number_of_sections;
	PIMAGE_SECTION_HEADER sections_headers;
	byte* pe_raw;
};
