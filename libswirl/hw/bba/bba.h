/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

struct ASIC;
struct VirtualNetwork;

MMIODevice* Create_BBA(ASIC* asic, VirtualNetwork* network);