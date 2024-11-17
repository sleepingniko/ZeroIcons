#pragma warning (disable : 6031) // I don't need the return value of getchar, I just want to use it to pause the program instead of immediately exiting.

#include <Windows.h>
#include <Psapi.h>
#include <stdio.h>

// TODO: Add support for patching the binary on the disk directly.
// This is extremely dangerous as it can prevent GD from starting entirely.
// So... if I add this, I have to be very careful and very precise.

// This is some VERY simple x86 assembly code that will force both the icon and color unlocked check to succeed.
// There's other ways of doing this, but this obviously the easiest.
CHAR g_pIconAndColorPatch[] =
{
	0xB0, 0x01,	// mov al, 1
	0x90,		// nop
	0x90,		// nop
	0x90		// nop
};

// Found via reverse engineering GeometryDash.exe.
// For future updates, patch diff'ing will make updating this hack extremely fast.
// I would use signatures, but uh... I don't feel like it.
// This offset is for icons.
#define ICONS_OFFSET 0x26F905

// This offset is for icon colors.
#define COLORS_OFFSET 0x89DCD

// This offset is for icon glow.
// Unfortunately, I DON'T HAVE A DAMN CLUE WHERE IT IS!!!!
#define GLOW_OFFSET 0

// The expected bytes at the offset above.
// If the bytes do not match, then bail out; it's likely that GD updated.
#define ICONS_ORIGINAL_BYTES 0x75C084FFF09BD6E8
#define COLORS_ORIGINAL_BYTES 0x74C084000EFAEEE8
#define GLOW_ORIGINAL_BYTES 0

// The patched bytes for the offsets above.
// If the bytes match, then that means the game is already patched and we can skip the patching process for that address.
#define ICONS_PATCHED_BYTES 0x75C08490909001B0
#define COLORS_PATCHED_BYTES 0x74C08490909001B0
#define GLOW_PATCHED_BYTES 0

void WritePatch(HANDLE hProcess, PVOID pTargetAddress, ULONG_PTR ExpectedBytes, ULONG_PTR PatchedBytes, CHAR* pPatchCode, SIZE_T PatchLength);

// Credits to...
// Absolute - The original 2.2 Unlock All/Icon Hack (which I reverse engineered; I could've done it my own way, but the code would've looked worse than it does now).
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

	PVOID pIconsAddress = 0;
	PVOID pColorsAddress = 0;

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
		getchar();
		return 1
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

	// This is genuinely disgusting, but does it look like I care?
	// hModule is the base address of GeometryDash.exe, and we add the offset to overwrite to it.
	// Typical game cheat stuff... except the fact that game cheats usually use signatures instead of hard-coded offsets.
	pIconsAddress = (PCHAR)(hModule)+ICONS_OFFSET;
	pColorsAddress = (PCHAR)(hModule)+COLORS_OFFSET;
	printf("[+] Icon Unlock Address: 0x%p\n[+] Color Unlock Address: 0x%p\n", pIconsAddress, pColorsAddress);

	// Patch the game!
	WritePatch(hProcess, pIconsAddress, ICONS_ORIGINAL_BYTES, ICONS_PATCHED_BYTES, g_pIconAndColorPatch, sizeof(g_pIconAndColorPatch));
	WritePatch(hProcess, pColorsAddress, COLORS_ORIGINAL_BYTES, COLORS_PATCHED_BYTES, g_pIconAndColorPatch, sizeof(g_pIconAndColorPatch));

	// ALL. DONE.
	printf("[+] Done! You may now close this window.\n");
	getchar();
	return 0;
}

void WritePatch(HANDLE hProcess, PVOID pTargetAddress, ULONG_PTR ExpectedBytes, ULONG_PTR PatchedBytes, CHAR* pPatchCode, SIZE_T PatchLength)
{
	ULONG_PTR Bytes = 0;

	// Read the process' memory for a future check.
	if (!ReadProcessMemory(hProcess, pTargetAddress, &Bytes, sizeof(Bytes), 0))
	{
		printf("[-] Failed to read process memory. Error: %d (0x%x)\n", GetLastError(), GetLastError());
		return;
	}

	// Uncomment the line below to get the 8 bytes located at the target address.
	// Useful for finding the expected bytes and the patched bytes.
	// printf("Address: 0x%p, Bytes: 0x%p\n", pTargetAddress, (PVOID)Bytes);

	// Before we check the expected bytes, we should check to see if the game was already patched.
	if (Bytes == PatchedBytes)
	{
		printf("[+] Looks like this offset is already patched! Skipping...\n");
		return;
	}

	// That "future check" is here.
	// Are the bytes equal to known good bytes? If not, then bail out!
	// If this fails, there's a high likelihood that Geometry Dash updated.
	if (Bytes != ExpectedBytes)
	{
		printf("[-] Failed to find the expected bytes. Did Geometry Dash update? Found Bytes: 0x%p\n", (PVOID)Bytes);
		return;
	}

	// And finally, we patch out the code so that the checks always succeed!
	if (!WriteProcessMemory(hProcess, pTargetAddress, pPatchCode, PatchLength, 0))
	{
		printf("[-] Failed to write process memory. Error: %d (0x%x)\n", GetLastError(), GetLastError());
		return;
	}
	printf("[+] Successfully patched address 0x%p!\n", pTargetAddress);
}