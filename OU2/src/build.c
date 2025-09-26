#include "../include/build.h"
#include "../include/parser.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

int build_target(const char *target_name, makefile *mf, bool force_rebuild,
                 bool silent) {

  rule *rule = makefile_rule(mf, target_name);

  if (!rule) {
    // No rule for target so check if it exists
    if (access(target_name, F_OK) == 0) {
      return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
  }

  const char **prereq = rule_prereq(rule);

  for (int i = 0; prereq[i] != NULL; i++) {
    if (build_target(prereq[i], mf, force_rebuild, silent) != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }

  struct stat target_stats;
  bool rebuild = false;

  if (stat(target_name, &target_stats) != 0) {
    rebuild = true;
  } else {
    for (int i = 0; prereq[i] != NULL; i++) {
      struct stat dep;
      if (stat(target_name, &dep) == 0)
        if (dep.st_mtime > target_stats.st_mtime)
          rebuild = true;
    }
  }

  if (rebuild) {
    char **cmd = rule_cmd(rule);
    run_build_cmd(cmd, target_name, silent);
  }

  return 1;
}

int run_build_cmd(char **cmd, const char *target_name, bool silent) {
  if (silent) {
  }

  int pid = fork();

  if (pid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }
  if (pid == 0) {
    execvp(cmd[0], cmd);
    perror("execvp");
    exit(EXIT_FAILURE);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "Command failed for target '%s'\n", target_name);
      return -1;
    }
  }

  return 1;
}
