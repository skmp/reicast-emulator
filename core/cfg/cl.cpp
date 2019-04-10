/*
	Command line parsing
	~yay~

	Nothing too interesting here, really
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "cfg/cfg.h"

wchar_t* trim_ws(wchar_t* str)
{
	if (str==0 || wcslen(str)==0)
		return 0;

	while(*str)
	{
		if (!isspace(*str))
			break;
		str++;
	}

	size_t l=wcslen(str);
	
	if (l==0)
		return 0;

	while(l>0)
	{
		if (!isspace(str[l-1]))
			break;
		str[l-1]=0;
		l--;
	}

	if (l==0)
		return 0;

	return str;
}

int setconfig(wchar_t** arg,int cl)
{
	int rv=0;
	for(;;)
	{
		if (cl<1)
		{
			wprintf(L"-config : invalid number of parameters, format is section:key=value\n");
			return rv;
		}
		wchar_t* sep=wcsstr(arg[1],L":");
		if (sep==0)
		{
			wprintf(L"-config : invalid parameter %s, format is section:key=value\n",arg[1]);
			return rv;
		}
		wchar_t* value=wcsstr(sep+1,L"=");
		if (value==0)
		{
			wprintf(L"-config : invalid parameter %s, format is section:key=value\n",arg[1]);
			return rv;
		}

		*sep++=0;
		*value++=0;

		wchar_t* sect=trim_ws(arg[1]);
		wchar_t* key=trim_ws(sep);
		value=trim_ws(value);

		if (sect==0 || key==0)
		{
			wprintf(L"-config : invalid parameter, format is section:key=value\n");
			return rv;
		}

		const wchar_t* constval = value;
		if (constval==0)
			constval=L"";
		wprintf(L"Virtual cfg %s:%s=%s\n",sect,key,value);

		cfgSetVirtual(sect,key,value);
		rv++;

		if (cl>=3 && wcsicmp(arg[2],L",")==0)
		{
			cl-=2;
			arg+=2;
			rv++;
			continue;
		}
		else
			break;
	}
	return rv;
}

#ifndef _ANDROID
#include "version.h"
#else
#define REICAST_VERSION "r7-android-tmp"
#endif

#if !defined(DEF_CONSOLE)
#if defined(linux) || defined(_ANDROID)
#define DEF_CONSOLE
#endif
#endif

#if defined(_WIN32)
#include <conio.h>
#endif

void cli_pause()
{
#ifdef DEF_CONSOLE
	return;
#endif

#if defined(_WIN32)
	wprintf(L"\nPress a key to exit.\n");
	_getch();
#else
	wprintf(L"\nPress enter to exit.\n");
	char c = getchar();
#endif
}



int showhelp(wchar_t** arg,int cl)
{
	wprintf(L"\nAvailable commands :\n");

	wprintf(L"-config	section:key=value [, ..]: add a virtual config value\n Virtual config values won't be saved to the .cfg file\n unless a different value is written to em\nNote :\n You can specify many settings in the xx:yy=zz , gg:hh=jj , ...\n format.The spaces between the values and ',' are needed.\n");
	wprintf(L"\n-help: show help info\n");
	wprintf(L"\n-version: show current version #\n\n");

	cli_pause();
	return 0;
}


int showversion(wchar_t** arg,int cl)
{
	wprintf(L"\nReicast Version: # %s built on %s \n", REICAST_VERSION, __DATE__);

	cli_pause();
	return 0;
}

bool ParseCommandLine(int argc,wchar_t* argv[])
{
	int cl=argc-2;
	wchar_t** arg=argv+1;
	while(cl>=0)
	{
		if (wcsicmp(*arg,L"-help")==0 || wcsicmp(*arg,L"--help")==0)
		{
			showhelp(arg,cl);
			return true;
		}
		if (wcsicmp(*arg,L"-version")==0 || wcsicmp(*arg,L"--version")==0)
		{
			showversion(arg,cl);
			return true;
		}
		else if (wcsicmp(*arg,L"-config")==0 || wcsicmp(*arg,L"--config")==0)
		{
			int as=setconfig(arg,cl);
			cl-=as;
			arg+=as;
		}
		else
		{
			wchar_t* extension = wcsrchr(*arg, '.');

			if (extension
				&& (wcsicmp(extension, L".cdi") == 0 || wcsicmp(extension, L".chd") == 0
					|| wcsicmp(extension, L".gdi") == 0 || wcsicmp(extension, L".lst") == 0))
			{
				wprintf(L"Using '%s' as cd image\n", *arg);
				cfgSetVirtual(L"config", L"image", *arg);
			}
			else if (extension && wcsicmp(extension, L".elf") == 0)
			{
				wprintf(L"Using '%s' as reios elf file\n", *arg);
				cfgSetVirtual(L"config", L"reios.enabled", L"1");
				cfgSetVirtual(L"reios", L"ElfFile", *arg);
			}
			else
			{
				wprintf(L"wtf %s is supposed to do ?\n",*arg);
			}
		}
		arg++;
		cl--;
	}
	wprintf(L"\n");
	return false;
}
