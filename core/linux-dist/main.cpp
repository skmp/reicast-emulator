#include "libreicast.h"

int main(int argc, char* argv[])
{
	libreicast_init(argc, argv);
	libreicast_run();
	libreicast_term();

	return 0;
}
