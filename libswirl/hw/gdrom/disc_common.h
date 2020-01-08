#pragma once
#include "imgread/imgread.h"

#include <vector>
using namespace std;

#include "deps/coreio/coreio.h"

extern u32 NullDriveDiscType;

bool DiscSwap();

int GetFile(char *szFileName);

void printtoc(TocInfo* toc,SessionInfo* ses);
extern u8 q_subchannel[96];

extern Disc* disc;


extern void gd_setdisc();
