#pragma once


#include "PeFile.h"


int isSehUsed(PeFile& peFile, DWORD allocatedBaseAddress);
int getAmountOfSehHandler(PeFile& peFile, DWORD allocatedBaseAddress);
void printSehChain();
PEXCEPTION_REGISTRATION_RECORD getLastSehHandler();
BOOL WINAPI MyRtlIsValidHandler(DWORD handler);
void hookRtlIsValidHandler();
void addToSehChain(void* where, PeFile& peFile, DWORD allocatedBaseAddress);
