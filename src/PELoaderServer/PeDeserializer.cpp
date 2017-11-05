
#include "PeDeserializer.h"
#include "Debug.h"


PeDeserializer::PeDeserializer(byte * pe_raw_data, PeFile* pe_obj)
{
	pe_file_ = pe_obj;
	pe_file_->pe_raw = pe_raw_data;
}

PeFile* PeDeserializer::Deserialize()
{
	LOG("PeDeserializer started deserialize");

	DosHeaderDeserialize();
	NtHeaderDeserialize();
	SectionsHeadersDeserialize();
	return GetPeFile();
}

bool PeDeserializer::DosHeaderDeserialize()
{
	CopyMemory(&(pe_file_->dos_header.e_magic), pe_file_->pe_raw, sizeof(pe_file_->dos_header));
	return true;
}

bool PeDeserializer::NtHeaderDeserialize()
{
	CopyMemory(&(pe_file_->nt_header.Signature),
		pe_file_->pe_raw + pe_file_->dos_header.e_lfanew, sizeof(pe_file_->nt_header.Signature));
	this->FileHeaderDeserialize();
	this->OptionalHeaderDeserialize();
	return true;
}

bool PeDeserializer::FileHeaderDeserialize()
{
	CopyMemory(&(pe_file_->nt_header.FileHeader),
		pe_file_->pe_raw + pe_file_->dos_header.e_lfanew + sizeof(pe_file_->nt_header.Signature),
		sizeof(pe_file_->nt_header.FileHeader));
	pe_file_->number_of_sections = pe_file_->nt_header.FileHeader.NumberOfSections;
	pe_file_->sections_headers = new IMAGE_SECTION_HEADER[pe_file_->number_of_sections];
	return true;
}

bool PeDeserializer::OptionalHeaderDeserialize()
{
	CopyMemory(&(pe_file_->nt_header.OptionalHeader),
		pe_file_->pe_raw + pe_file_->dos_header.e_lfanew + sizeof(pe_file_->nt_header.Signature) 
		+ sizeof(pe_file_->nt_header.FileHeader),
		pe_file_->nt_header.FileHeader.SizeOfOptionalHeader);
	return true;
}

bool PeDeserializer::SectionsHeadersDeserialize()
{
	for (int i = 0; i < pe_file_->number_of_sections; ++i) 
	{
		CopyMemory(&(pe_file_->sections_headers[i]),
			pe_file_->pe_raw + pe_file_->dos_header.e_lfanew + sizeof(pe_file_->nt_header.Signature) +
			sizeof(pe_file_->nt_header.FileHeader) + pe_file_->nt_header.FileHeader.SizeOfOptionalHeader +
			i * sizeof(IMAGE_SECTION_HEADER),
			sizeof(IMAGE_SECTION_HEADER));
	}
	return true;
}

PeFile* PeDeserializer::GetPeFile()
{
	return pe_file_;
}
