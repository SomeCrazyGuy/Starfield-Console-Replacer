#pragma once

#include "main.h"

extern void ReadConfigFile();
extern void BindConfigInt(const char* name, int* value);
extern void SaveConfigFile();
#define CONFIG_INT(X) BindConfigInt(#X,&X)