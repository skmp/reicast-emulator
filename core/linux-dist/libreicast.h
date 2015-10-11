#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern int libreicast_init(int argc, char* argv[]);
extern int libreicast_init(int argc, char* argv[], void* hwnd);
extern int libreicast_run();
extern int libreicast_term();

#ifdef __cplusplus
}
#endif
