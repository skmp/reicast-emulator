#include "hw/sh4/sh4_core.h"
#include "hw/sh4/dyna/ngen.h"

int cycle_counter;

void ngen_GetFeatures(ngen_features* dst)
{
	dst->InterpreterFallback = false;
	dst->OnlyDynamicEnds = false;
}
