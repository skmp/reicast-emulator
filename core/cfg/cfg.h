#pragma once

#include "types.h"
/*
**	cfg* prototypes, if you pass NULL to a cfgSave* it will wipe out the section
**	} if you pass it to lpKey it will wipe out that particular entry
**	} if you add write to something it will create it if its not present
**	} ** Strings passed to LoadStr should be MAX_PATH in size ! **
*/

bool cfgOpen();
s32   cfgLoadInt(const wchar_t * lpSection, const wchar_t * lpKey,s32 Default);
s32   cfgGameInt(const wchar_t * lpSection, const wchar_t * lpKey,s32 Default);
void  cfgSaveInt(const wchar_t * lpSection, const wchar_t * lpKey, s32 Int);
void  cfgLoadStr(const wchar_t * lpSection, const wchar_t * lpKey, wchar_t * lpReturn,const wchar_t* lpDefault);
wstring  cfgLoadStr(const wchar_t * Section, const wchar_t * Key, const wchar_t* Default);
void  cfgSaveStr(const wchar_t * lpSection, const wchar_t * lpKey, const wchar_t * lpString);
void  cfgSaveBool(const wchar_t * Section, const wchar_t * Key, bool BoolValue);
bool  cfgLoadBool(const wchar_t * Section, const wchar_t * Key,bool Default);
s32  cfgExists(const wchar_t * Section, const wchar_t * Key);
void cfgSetVirtual(const wchar_t * lpSection, const wchar_t * lpKey, const wchar_t * lpString);

bool ParseCommandLine(int argc,wchar_t* argv[]);
