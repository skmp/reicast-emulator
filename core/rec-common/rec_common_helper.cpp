#include "types.h"

unsigned int ngen_required = true;

void ngen_terminate()
{
	ngen_required = false;
	printf("ngen thread stopped\n");
}
