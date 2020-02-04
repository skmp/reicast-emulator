/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "imgread/imgread.h"

#include <vector>
using namespace std;

#include "deps/coreio/coreio.h"

extern u32 NullDriveDiscType;

int GetFile(char *szFileName);

void printtoc(TocInfo* toc,SessionInfo* ses);
extern u8 q_subchannel[96];
