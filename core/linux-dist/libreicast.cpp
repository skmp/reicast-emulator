#include "libreicast.h"
#include "linux.h"

int libreicast_init(int argc, char* argv[])
{
	return reicast_init(argc, argv);
}

int libreicast_run()
{
	return reicast_run();
}

int libreicast_term()
{
	return reicast_term();
}
