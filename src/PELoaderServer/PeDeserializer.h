#pragma once

#include "PeFile.h"


class PeDeserializer {

public:
	PeDeserializer(byte *, PeFile*);
	PeFile* Deserialize();
	PeFile* GetPeFile();
private:
	PeFile* pe_file_;

	bool DosHeaderDeserialize();
	bool NtHeaderDeserialize();
	bool FileHeaderDeserialize();
	bool OptionalHeaderDeserialize();
	bool SectionsHeadersDeserialize();
};
