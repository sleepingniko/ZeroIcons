#pragma warning (disable : 6031) // I don't need the return value of getchar, I just want to use it to pause the program instead of immediately exiting.

#include <Windows.h>
#include <Psapi.h>
#include <stdio.h>

// TODO: Add support for patching the binary on the disk directly.
// This is extremely dangerous as it can prevent GD from starting entirely.
// So... if I add this, I have to be very careful and very precise.

// This is some VERY simple x86 assembly code that will force the icon unlocked check to succeed.
// I'm sure there's another way you can achieve this result (e.g. an injection-based hack abusing hooks), but this is my way of doing it.
unsigned char g_pCodePatch[] =
{
	0xB0, 0x01,	// mov al, 1
	0x90,		// nop
	0x90,		// nop
	0x90		// nop
};

// Found via reverse engineering GeometryDash.exe.
// For future updates, patch diff'ing will make updating this hack extremely fast.
#define ICON_HACK_PATCH_OFFSET 0x26F905

// The expected bytes at the offset above.
// If the bytes do not match, then bail out; it's likely that GD updated.
#define ICON_HACK_ORIGINAL_BYTES 0x75C084FFF09BD6E8

// Credits to...
// Absolute - The original 2.2 Unlock All/Icon Hack (which I reverse engineered; I could've done it my own way, but the code would've looked HIDEOUS).
int main(int argc, char** argv)
{
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);

	HWND hWindow = 0;

	DWORD ProcessID = 0;
	HANDLE hProcess = 0;

	HMODULE* phModules = 0;
	DWORD BytesReturned = 0;

	CHAR pBaseName[MAX_PATH] = { 0 };

	HMODULE hModule = 0;
	PVOID pTargetAddress = 0;
	SIZE_T ReadMemory = 0;

	// I mean, come on, I HAVE to do this.
	SetConsoleTitleA("ZeroIcons Icon Hack");
	printf("[*] ZeroIcons 2.2 Icon Hack\n[*] Written by The Exploit\n\n");

	// Find the window for the Geometry Dash process.
	// We can then use this window handle to get its process ID.
	printf("[*] Finding the process window...\n");
	while (!hWindow)
	{
		hWindow = FindWindowA(0, "Geometry Dash");
		Sleep(100);
	}
	printf("[+] Window Handle: 0x%p\n", hWindow);

	// As stated previously, get the process' ID.
	if (!GetWindowThreadProcessId(hWindow, &ProcessID))
	{
		printf("[-] Failed to get the process ID. Error: %d (0x%x)\n", GetLastError(), GetLastError());
		getchar();
		return 1;
	}
	printf("[+] Process ID: %d (0x%x)\n", ProcessID, ProcessID);

	// Create a handle to the process, and open it with the specified access below so this hack can patch the game's memory.
	hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, 0, ProcessID);
	if (!hProcess)
	{
		printf("[-] Failed to create a process handle. Error: %d (0x%x)\n", GetLastError(), GetLastError());
		getchar();
		return 1;
	}
	printf("[+] Process Handle: 0x%p\n", hProcess);

	// Get the number of bytes required.
	K32EnumProcessModules(hProcess, 0, 0, &BytesReturned);

	// Getting the number of bytes required, the program can now allocate enough memory for the module array.
	phModules = malloc(BytesReturned);
	if (!phModules)
	{
		printf("[-] Failed to allocate heap memory.\n");
		getchar();
		return 1;
	}

	// Enumerate the process modules to find GeometryDash.exe's base address.
	if (!K32EnumProcessModules(hProcess, phModules, BytesReturned, &BytesReturned))
	{
		printf("[-] Failed to enumerate process modules. Error: %d (0x%x)\n", GetLastError(), GetLastError());
	}

	// Iterate over the module array to find the correct base address.
	for (DWORD i = 0; i < BytesReturned / sizeof(HMODULE); i++)
	{
		// Zero out the base name string each time to ensure no artifacts remain.
		memset(pBaseName, 0, sizeof(pBaseName));

		// Get the module name of the provided base address.
		if (!K32GetModuleBaseNameA(hProcess, phModules[i], pBaseName, sizeof(pBaseName)))
		{
			printf("[-] Failed to enumerate module base name. Error: %d (0x%x). Continuing...\n", GetLastError(), GetLastError());
			continue;
		}

		// Is the base name "GeometryDash.exe"?
		if (!strcmp(pBaseName, "GeometryDash.exe"))
		{
			hModule = phModules[i];
			break;
		}
	}
	// Bruh.
	if (!hModule)
	{
		printf("[-] Failed to find the process' base address.\n");
		getchar();
		return 1;
	}
	printf("[+] Base Address: 0x%p\n", hModule);

	// This is genuinely disgusting, but C might be more disgusting at times.
	// hModule is the base address of GeometryDash.exe, and we add the offset to overwrite to it.
	// Typical game cheat stuff... except the fact that game cheats usually use signatures instead of hard-coded offsets.
	pTargetAddress = (PCHAR)(hModule)+ICON_HACK_PATCH_OFFSET;
	printf("[+] Overwrite Address: 0x%p\n", pTargetAddress);

	// Read the process' memory for a future check.
	if (!ReadProcessMemory(hProcess, pTargetAddress, &ReadMemory, sizeof(ReadMemory), 0))
	{
		printf("[-] Failed to read process memory. Error: %d (0x%x)\n", GetLastError(), GetLastError());
		getchar();
		return 1;
	}
	// That "future check" is here.
	// Are the bytes equal to known good bytes? If not, then bail out!
	// If this fails, there's a high likelihood that Geometry Dash updated.
	if (ReadMemory != ICON_HACK_ORIGINAL_BYTES)
	{
		printf("[-] Failed to find the expected bytes. Did Geometry Dash update? Found Bytes: 0x%p\n", (PVOID)ReadMemory);
		getchar();
		return 1;
	}

	// And finally, we write our beautiful 5 bytes worth of assembly instructions.
	// All that work... just for this.
	// What the fuck.
	if (!WriteProcessMemory(hProcess, pTargetAddress, g_pCodePatch, sizeof(g_pCodePatch), 0))
	{
		printf("[-] Failed to write process memory. Error: %d (0x%x)\n", GetLastError(), GetLastError());
		getchar();
		return 1;
	}

	// ALL. DONE.
	printf("[+] Patched! You may now close this window.\n");
	getchar();
	return 0;
}
