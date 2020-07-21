
#include <windows.h>


size_t strlen(char* str)
{
	char* s;
	for (s = str; *s; ++s);
	return (s - str);
}


void printString(const char* message)
{
	auto console = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD out = 0;
	auto result = WriteConsoleA(console, message, strlen(message), &out, nullptr);
}

void testFunction()
{
    printString("testFunction called\n");
    __try
    {
        printString("Raising exception...\n");
        RaiseException(1,0,0, NULL);
    }
    __finally
    {
        printString(" __finally called!\n");
    }
}