
#include <Windows.h>

bool JitMagicInit();
void RunTests();

typedef LPVOID
(WINAPI*fnVirtualAlloc)(
LPVOID lpAddress,
SIZE_T dwSize,
DWORD flAllocationType,
DWORD flProtect
);

typedef BOOL
(WINAPI*fnVirtualFree)(
LPVOID lpAddress,
SIZE_T dwSize,
DWORD dwFreeType
);

typedef BOOL
(WINAPI*fnVirtualProtect)(
LPVOID lpAddress,
SIZE_T dwSize,
DWORD flNewProtect,
PDWORD lpflOldProtect
);


typedef LPVOID
(WINAPI*fnMapViewOfFileEx)(
_In_ HANDLE hFileMappingObject,
_In_ DWORD dwDesiredAccess,
_In_ DWORD dwFileOffsetHigh,
_In_ DWORD dwFileOffsetLow,
_In_ SIZE_T dwNumberOfBytesToMap,
_In_opt_ LPVOID lpBaseAddress
);


typedef 
HANDLE
(WINAPI*fnCreateFileMappingW)(
HANDLE hFile,
LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
DWORD flProtect,
DWORD dwMaximumSizeHigh,
DWORD dwMaximumSizeLow,
LPCWSTR lpName
);

extern fnVirtualAlloc rtVirtualAlloc;
extern fnVirtualFree rtVirtualFree;
extern fnVirtualProtect rtVirtualProtect;
extern fnMapViewOfFileEx rtMapViewOfFileEx;
extern fnCreateFileMappingW rtCreateFileMappingW;

#define VirtualAlloc rtVirtualAlloc
#define VirtualFree rtVirtualFree
#define VirtualProtect rtVirtualProtect
#define MapViewOfFileEx rtMapViewOfFileEx
#define CreateFileMappingW rtCreateFileMappingW