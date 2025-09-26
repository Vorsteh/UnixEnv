#include "../include/build.h"
#include "../include/parser.h"

int build_target(const char *target_name, makefile *mf, bool force_rebuild,
                 bool silent) {

  rule *rule = makefile_rule(mf, target_name);

  const char **prereq = rule_prereq(rule);
  char **cmd = rule_cmd(rule);

  for (int i = 0; prereq[i] != NULL; i++) {
    printf("Depends on: %s\n", prereq[i]);
  }

  for (int i = 0; cmd[i] != NULL; i++) {
    printf("Command part: %s\n", cmd[i]);
  }
  printf("%d %d", silent, force_rebuild);

  return 1;
}
