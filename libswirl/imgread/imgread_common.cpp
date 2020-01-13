#include "imgread_common.h"

Disc* chd_parse(const wchar* file);
Disc* gdi_parse(const wchar* file);
Disc* cdi_parse(const wchar* file);
Disc* cue_parse(const wchar* file);
#if HOST_OS==OS_WINDOWS
Disc* ioctl_parse(const wchar* file);
#endif

Disc* (*imgread_drivers[])(const wchar* path) =
{
	chd_parse,
	gdi_parse,
	cdi_parse,
	cue_parse,
#if HOST_OS==OS_WINDOWS
	ioctl_parse,
#endif
	0
};

DiscType GuessDiscType(bool m1, bool m2, bool da)
{
	if ((m1 == true) && (da == false) && (m2 == false))
		return  CdRom;
	else if (m2)
		return  CdRom_XA;
	else if (da && m1)
		return CdRom_Extra;
	else
		return CdRom;
}


Disc* OpenDisc(const wchar* fn)
{
	Disc* rv = NULL;

	for (unat i = 0; imgread_drivers[i] && !rv; i++) {  // ;drivers[i] && !(rv=drivers[i](fn));
		rv = imgread_drivers[i](fn);
	}

	return rv;
}