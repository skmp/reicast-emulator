#pragma once

#include "types.h"

#include "oslib/context.h"

void context_from_segfault(rei_host_context_t* reictx, void* segfault_ctx);
void context_to_segfault(rei_host_context_t* reictx, void* segfault_ctx);
