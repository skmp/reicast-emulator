#include "aica_mem.h"

//Map using _vmem .. yay
void aica_init_mem()
{
	aica_ram.data[ARAM_SIZE-1]=1;
	aica_ram.Zero();
}
//kill mem map & free used mem ;)
void aica_term_mem()
{

}

