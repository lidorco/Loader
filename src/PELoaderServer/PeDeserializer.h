#pragma once

#include "PeFile.h"


class PeDeserializer {

public:
	PeDeserializer(byte *, PeFile*);
	PeFile* deserialize();
	PeFile* getPeFile();
private:
	PeFile* m_pe_file;

	bool dosHeaderDeserialize();
	bool ntHeaderDeserialize();
	bool fileHeaderDeserialize();
	bool optionalHeaderDeserialize();
	bool sectionsHeadersDeserialize();
};
