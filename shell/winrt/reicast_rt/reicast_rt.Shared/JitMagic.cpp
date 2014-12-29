/*
	Adepted from http://forum.xda-developers.com/showthread.php?t=1944675
	Original code by mamaich
	Modified by skmp@nillware.com
*/

#include "pch.h"
#include "JitMagic.h"

PIMAGE_NT_HEADERS WINAPI ImageNtHeader(PVOID Base)
{
	return (PIMAGE_NT_HEADERS)
		((LPBYTE)Base + ((PIMAGE_DOS_HEADER)Base)->e_lfanew);
}

PIMAGE_SECTION_HEADER WINAPI RtlImageRvaToSection(const IMAGE_NT_HEADERS *nt,
	HMODULE module, DWORD_PTR rva)
{
	int i;
	const IMAGE_SECTION_HEADER *sec;

	sec = (const IMAGE_SECTION_HEADER*)((const char*)&nt->OptionalHeader +
		nt->FileHeader.SizeOfOptionalHeader);
	for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
	{
		if ((sec->VirtualAddress <= rva) && (sec->VirtualAddress + sec->SizeOfRawData > rva))
			return (PIMAGE_SECTION_HEADER)sec;
	}
	return NULL;
}

PVOID WINAPI RtlImageRvaToVa(const IMAGE_NT_HEADERS *nt, HMODULE module,
	DWORD_PTR rva, IMAGE_SECTION_HEADER **section)
{
	IMAGE_SECTION_HEADER *sec;

	if (section && *section)  /* try this section first */
	{
		sec = *section;
		if ((sec->VirtualAddress <= rva) && (sec->VirtualAddress + sec->SizeOfRawData > rva))
			goto found;
	}
	if (!(sec = RtlImageRvaToSection(nt, module, rva))) return NULL;
found:
	if (section) *section = sec;
	return (char *)module + sec->PointerToRawData + (rva - sec->VirtualAddress);
}

PVOID WINAPI ImageDirectoryEntryToDataEx(PVOID base, BOOLEAN image, USHORT dir, PULONG size, PIMAGE_SECTION_HEADER *section)
{
	const IMAGE_NT_HEADERS *nt;
	DWORD_PTR addr;

	*size = 0;
	if (section) *section = NULL;

	if (!(nt = ImageNtHeader(base))) return NULL;
	if (dir >= nt->OptionalHeader.NumberOfRvaAndSizes) return NULL;
	if (!(addr = nt->OptionalHeader.DataDirectory[dir].VirtualAddress)) return NULL;

	*size = nt->OptionalHeader.DataDirectory[dir].Size;
	if (image || addr < nt->OptionalHeader.SizeOfHeaders) return (char *)base + addr;

	return RtlImageRvaToVa(nt, (HMODULE)base, addr, section);
}

// == Windows API GetProcAddress
void *PeGetProcAddressA(void *Base, LPCSTR Name)
{
	DWORD Tmp;

	IMAGE_NT_HEADERS *NT = ImageNtHeader(Base);
	IMAGE_EXPORT_DIRECTORY *Exp = (IMAGE_EXPORT_DIRECTORY*)ImageDirectoryEntryToDataEx(Base, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &Tmp, 0);
	if (Exp == 0 || Exp->NumberOfFunctions == 0)
	{
		SetLastError(ERROR_NOT_FOUND);
		return 0;
	}

	DWORD *Names = (DWORD*)(Exp->AddressOfNames + (DWORD_PTR)Base);
	WORD *Ordinals = (WORD*)(Exp->AddressOfNameOrdinals + (DWORD_PTR)Base);
	DWORD *Functions = (DWORD*)(Exp->AddressOfFunctions + (DWORD_PTR)Base);

	FARPROC Ret = 0;

	if ((DWORD_PTR)Name<65536)
	{
		if ((DWORD_PTR)Name - Exp->Base<Exp->NumberOfFunctions)
			Ret = (FARPROC)(Functions[(DWORD_PTR)Name - Exp->Base] + (DWORD_PTR)Base);
	}
	else
	{
		for (DWORD i = 0; i<Exp->NumberOfNames && Ret == 0; i++)
		{
			char *Func = (char*)(Names[i] + (DWORD_PTR)Base);
			if (Func && strcmp(Func, Name) == 0)
				Ret = (FARPROC)(Functions[Ordinals[i]] + (DWORD_PTR)Base);
		}
	}

	if (Ret)
	{
		DWORD ExpStart = NT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress + (DWORD)Base;
		DWORD ExpSize = NT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
		if ((DWORD)Ret >= ExpStart && (DWORD)Ret <= ExpStart + ExpSize)
		{
			// Forwarder
			return 0;
		}
		return Ret;
	}

	return 0;
}

fnVirtualAlloc rtVirtualAlloc;
fnVirtualFree rtVirtualFree;
fnVirtualProtect rtVirtualProtect;
fnMapViewOfFileEx rtMapViewOfFileEx;
fnCreateFileMappingW rtCreateFileMappingW;

/*
	VirtualAlloc
	VirtualProtect
	VirtualFree
*/

bool JitMagicInit() {
	char *Tmp = (char*)GetTickCount64;
	Tmp = (char*)((~0xFFF)&(DWORD_PTR)Tmp);

	while (Tmp)
	{
		__try
		{
			if (Tmp[0] == 'M' && Tmp[1] == 'Z')
				break;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		Tmp -= 0x1000;
	}

	if (Tmp == 0)
		return false;

	rtVirtualAlloc = (fnVirtualAlloc)PeGetProcAddressA(Tmp, "VirtualAlloc");
	rtVirtualFree = (fnVirtualFree)PeGetProcAddressA(Tmp, "VirtualFree");
	rtVirtualProtect = (fnVirtualProtect)PeGetProcAddressA(Tmp, "VirtualProtect");
	rtMapViewOfFileEx = (fnMapViewOfFileEx)PeGetProcAddressA(Tmp, "MapViewOfFileEx");
	rtCreateFileMappingW = (fnCreateFileMappingW)PeGetProcAddressA(Tmp, "CreateFileMappingW");

	return rtVirtualAlloc != 0 && rtVirtualFree != 0 && rtVirtualProtect != 0 && rtMapViewOfFileEx != 0 && rtCreateFileMappingW != 0;
}


#include <assert.h>
void RunTests() {
	if (JitMagicInit()) {
		void* code = rtVirtualAlloc(0, 512 * 1024 * 124, MEM_RESERVE, PAGE_NOACCESS);

		auto rva = rtVirtualAlloc(code, 4096, MEM_COMMIT, PAGE_NOACCESS);

		assert(rva != NULL);

		DWORD old;
		auto rvb = rtVirtualProtect(code, 4096, PAGE_READWRITE, &old);

		assert(rvb != FALSE);

		typedef int add3();

		auto p8 = (int8_t*)code;

#if 0
		*p8++ = 0x5;

		*p8++ = 0x3;
		*p8++ = 0x0;
		*p8++ = 0x0;
		*p8++ = 0x0;

		*p8++ = 0xC3;
#else
		*p8++ = 0x70;
		*p8++ = 0x47;

		*p8++ = 0xE1;
		*p8++ = 0x1E;
		*p8++ = 0xFF;
		*p8++ = 0x2F;
#endif
		auto rvc = rtVirtualProtect(code, 4096, 0x10, &old);

		assert(rvc != FALSE);


		auto fn = (add3*)code;

		int rv = fn();

		rtVirtualFree(code, 512 * 1024 * 1024, MEM_RELEASE);
	}
}