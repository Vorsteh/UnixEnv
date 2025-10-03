

#ifndef BUILD_H
#define BUILD_H

#include "parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

/*
 * build_target - Builds the target from the makefile
 *
 * @param traget_name       Name of target
 * @param mf                The makefile
 * @param force_rebuild     Boolean value stating if it should force the build
 * or not.
 * @param silent            Boolean value stating whether to have any output or
 * not.
 *
 * @reuturn int EXIT_SUCCESS on correct execution
 * */
int build_target(const char *target_name, makefile *mf, bool force_rebuild,
                 bool silent);
/*
 * run_build_cmd - Runs the commands to build the target
 *
 * @param cmd               Array of commands to be executed
 * @param target_name       The name of the target
 * @param silent            Boolean value stating whether to have any output or
 * not
 *
 * @reutnr void
 * */
int run_build_cmd(char **cmd, const char *target_name, bool silent);
#endif
