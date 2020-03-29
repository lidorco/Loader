
#include "PeDeserializer.h"
#include "Debug.h"


PeDeserializer::PeDeserializer(byte * pe_raw_data, PeFile* pe_obj)
{
	m_pe_file = pe_obj;
	m_pe_file->pe_raw = pe_raw_data;
}

PeFile* PeDeserializer::deserialize()
{
	LOG("PeDeserializer started deserialize");

	dosHeaderDeserialize();
	ntHeaderDeserialize();
	sectionsHeadersDeserialize();
	return getPeFile();
}

bool PeDeserializer::dosHeaderDeserialize()
{
	memcpy(&(m_pe_file->dos_header.e_magic), m_pe_file->pe_raw, sizeof(m_pe_file->dos_header));
	return true;
}

bool PeDeserializer::ntHeaderDeserialize()
{
	memcpy(&(m_pe_file->nt_header.Signature), 
		m_pe_file->pe_raw + m_pe_file->dos_header.e_lfanew, sizeof(m_pe_file->nt_header.Signature));
	this->fileHeaderDeserialize();
	this->optionalHeaderDeserialize();
	return true;
}

bool PeDeserializer::fileHeaderDeserialize()
{
	memcpy(&(m_pe_file->nt_header.FileHeader),
		m_pe_file->pe_raw + m_pe_file->dos_header.e_lfanew + sizeof(m_pe_file->nt_header.Signature),
		sizeof(m_pe_file->nt_header.FileHeader));
	m_pe_file->number_of_sections = m_pe_file->nt_header.FileHeader.NumberOfSections;
	m_pe_file->sections_headers = new IMAGE_SECTION_HEADER[m_pe_file->number_of_sections];
	return true;
}

bool PeDeserializer::optionalHeaderDeserialize()
{
	memcpy(&(m_pe_file->nt_header.OptionalHeader), 
		m_pe_file->pe_raw + m_pe_file->dos_header.e_lfanew + sizeof(m_pe_file->nt_header.Signature) 
		+ sizeof(m_pe_file->nt_header.FileHeader),
		m_pe_file->nt_header.FileHeader.SizeOfOptionalHeader);
	return true;
}

bool PeDeserializer::sectionsHeadersDeserialize()
{
	for (int i = 0; i < m_pe_file->number_of_sections; ++i) 
	{
		memcpy(&(m_pe_file->sections_headers[i]),
			m_pe_file->pe_raw + m_pe_file->dos_header.e_lfanew + sizeof(m_pe_file->nt_header.Signature) +
			sizeof(m_pe_file->nt_header.FileHeader) + m_pe_file->nt_header.FileHeader.SizeOfOptionalHeader +
			i * sizeof(IMAGE_SECTION_HEADER),
			sizeof(IMAGE_SECTION_HEADER));
	}
	return true;
}

PeFile* PeDeserializer::getPeFile()
{
	return m_pe_file;
}
