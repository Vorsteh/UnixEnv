

#ifndef BUILD_H
#define BUILD_H

#include "../include/parser.h"
#include <stdbool.h>
#include <stdio.h>

int build_target(const char *target_name, makefile *mf, bool force_rebuild,
                 bool silent);

#endif
