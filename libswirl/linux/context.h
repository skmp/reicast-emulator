/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "types.h"

#include "oslib/context.h"

void segfault_store(void* segfault_ctx);
void segfault_load(void* segfault_ctx);
void segfault_set_pc(void* segfault_ctx, unat new_pc, unat* old_pc);

void context_from_segfault(rei_host_context_t* reictx);
void context_to_segfault(rei_host_context_t* reictx);

