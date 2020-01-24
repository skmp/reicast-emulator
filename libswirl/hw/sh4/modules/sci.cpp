/*
	SCI not implemented
*/

#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "modules.h"

struct Sh4ModSci_impl : Sh4ModSci
{
	SuperH4Mmr* sh4mmr;
	//Init term res
	Sh4ModSci_impl(SuperH4Mmr* sh4mmr) : sh4mmr(sh4mmr)
	{
	}

	void Reset()
	{
	}

};

Sh4ModSci* Sh4ModSci::Create(SuperH4Mmr* sh4mmr) {
	return new Sh4ModSci_impl(sh4mmr);
}