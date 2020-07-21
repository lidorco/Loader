
#include <windows.h>


size_t strlen(char* str)
{
	char* s;
	for (s = str; *s; ++s);
	return (s - str);
}


void printf(char* message)
{
	auto console = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD out = 0;
	auto result = WriteConsoleA(console, message, strlen(message), &out, nullptr);
}

void testFunction()
{
	printf("TestDll::testFunction Running successfully\n");
}


int add(int a, int b)
{
	printf("TestDll::add Running successfully\n");
	return a + b;
}

int multiplication(int a, int b)
{
	printf("TestDll::multiplication Running successfully\n");
	return a * b;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		printf("TestDll::DllMain - reason is DLL_PROCESS_ATTACH\n");
		break;

	case DLL_THREAD_ATTACH:
		printf ("TestDll::DllMain - reason is DLL_THREAD_ATTACH\n");
		break;

	case DLL_THREAD_DETACH:
		printf ("TestDll::DllMain - reason is DLL_THREAD_DETACH\n");
		break;

	case DLL_PROCESS_DETACH:
		printf("TestDll::DllMain - reason is DLL_PROCESS_DETACH\n");
		break;
	}
	return TRUE;
}
