#pragma once

#include "../betterapi.h"

#ifdef __cplusplus
#define CEXPORT extern "C"
#else
#define CEXPORT extern
#endif

// use minhook to hook old_func and redirect to new_func, return a pointer to call old_func
// returns null on error
CEXPORT FUNC_PTR minhook_hook_function(FUNC_PTR old_func, FUNC_PTR new_func);