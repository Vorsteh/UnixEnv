

#ifndef BUILD_H
#define BUILD_H

#include "parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

int build_target(const char *target_name, makefile *mf, bool force_rebuild,
                 bool silent);

int run_build_cmd(char **cmd, const char *target_name, bool silent);
#endif
