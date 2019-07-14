#pragma once

#include "types.h"

void dc_loadstate();
void dc_savestate();
void dc_stop();
void dc_reset();
void dc_resume();
void dc_term();
int dc_start_game(const char *path);

void* dc_run(void*);

void dc_request_reset();

// TODO: rename these
int reicast_init(int argc, char* argv[]);
