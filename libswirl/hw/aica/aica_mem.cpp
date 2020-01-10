#include "aica_mem.h"

//Map using _vmem .. yay
void aica_init_mem()
{
	memset(aica_reg,0,sizeof(aica_reg));
	aica_ram.data[ARAM_SIZE-1]=1;
	aica_ram.Zero();
}
//kill mem map & free used mem ;)
void aica_term_mem()
{

}

