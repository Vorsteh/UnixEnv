#include "build.h"
#include "parser.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

int build_target(const char *target_name, makefile *mf, bool force_rebuild,
                 bool silent) {

  // Gather the rule using the provided parsing functions
  rule *rule = makefile_rule(mf, target_name);

  if (!rule) {
    // No rule for target so check if file exists
    if (access(target_name, F_OK) == 0) {
      return EXIT_SUCCESS;
    }

    fprintf(stderr, "mmake: No rule to make target '%s'\n", target_name);
    return EXIT_FAILURE;
  }

  // Gather pre requestits from provided parsing functions
  const char **prereq = rule_prereq(rule);

  for (int i = 0; prereq[i] != NULL; i++) {
    // Recursvily build each target starting fromt he top
    if (build_target(prereq[i], mf, force_rebuild, silent) != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }

  struct stat target_stats;
  bool rebuild = false;

  // Check whether file is up to date or dosent exist then set boolean rebuild
  // based on data
  if (stat(target_name, &target_stats) != 0) {
    rebuild = true;
  } else {
    for (int i = 0; prereq[i] != NULL; i++) {
      struct stat dep;
      if (stat(prereq[i], &dep) == 0)
        if (dep.st_mtime > target_stats.st_mtime) {
          rebuild = true;
          break;
        }
    }
  }

  // If it needs to be rebuilt or force_rebuild is set then run each build cmd
  if (rebuild || force_rebuild) {
    char **cmd = rule_cmd(rule);
    if (run_build_cmd(cmd, target_name, silent) != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

int run_build_cmd(char **cmd, const char *target_name, bool silent) {
  // If not silent is set print each cmd ran
  if (!silent) {
    for (int i = 0; cmd[i] != NULL; i++) {
      printf("%s", cmd[i]);
      // If it is not the last cmd print space
      if (cmd[i + 1] != NULL) {
        printf(" ");
      }
    }
    printf("\n");
    fflush(stdout);
  }

  // Fork a child
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return EXIT_FAILURE;
  }

  // If child procecc execute the cmd
  if (pid == 0) {
    execvp(cmd[0], cmd);
    perror("execvp");
    exit(EXIT_FAILURE);
  }

  // Wait for the child
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
    return EXIT_FAILURE;
  }

  // Check that the child exited correctly
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "mmake: Command failed for target '%s'\n", target_name);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
