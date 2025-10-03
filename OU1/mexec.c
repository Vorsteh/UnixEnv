/**
 * Executes a series of commands using a pipeline
 *
 * @file mexec.c
 * @author Emil Jönsson @c24ejn
 * @version 1.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

#define MAX_LINE 1024
// Functions

/*
 * handleCmdLineArgs - Makes sure program has correct amount of arguments
 *
 * @param argc    number of command arguments
 * @param *argv[] Aarray of command arugments
 *
 * Returns: FILE * to the opend file or stdin
 *          Returns NULL on error
 * */
FILE *handle_CmdLine_Args(int argc, char *argv[]);

/*
 * parse_line - Splits lines into array of arguments
 *
 * @param buffer  String containting arguments
 *
 * Returns: Array of strings based on all arguments in main string
 *
 * */

char **parse_line(char *buffer);

/*
 * free_args - Frees memory allocated by malloc and realloc
 *
 * @param args Array of string argumetns
 *
 * @return void
 */
void free_args(char **args);

/*
 * commands_setup - Setups all commands from input file or stdin and stores them
 * in an dynamically allocated array
 *
 * @param file              Input file/stdin
 * @param commands_amount   Pointer to commands_amount that keeps track of
 * amount of commands
 *
 * @return Dynamically allocated array of arguments array
 */

char ***commands_setup(FILE *file, int *commands_amount);

/*
 * pipes_setup - Setups pipes for communcation between processes through a
 * pipeline
 *
 * @param commands_amount   The amount of commands
 *
 * @return 2D array of file descriptors
 *
 * */
int **pipes_setup(int commands_amount);

/*
 * fork_setup - Forks a new process for each command and setups the pipeline by
 * redirecting the child processes stdin and stdout
 *
 * @param commands          Array of commands
 * @param commands_amount   Number of commands
 * @param pipes             Array of pipes
 *
 * Allocates memory which should be freed using the cleanup function.
 *
 * @return void
 * */
void fork_setup(char ***commands, int commands_amount, int **pipes);

/*
 * wait_for_children - Waits for child processes and checks exit status
 *
 * @param commands_amount  Number of commands
 *
 * @return void
 *
 * */
void wait_for_children(int commands_amount);

/*
 * cleanup - Cleans up all dynamically allocated memory and closes all files and
 * pipes
 *
 * @param file              Input file
 * @param commands          Array of commands
 * @param commands_amount   Amount of commands
 * @param pipes             Array of pipes
 *
 * @reutnr void
 * */
void cleanup(FILE *file, char ***commands, int commands_amount, int **pipes);

int main(int argc, char *argv[]) {

  FILE *file = handle_CmdLine_Args(argc, argv);

  if (!file)
    return EXIT_FAILURE;

  int commands_amount = 0;

  char ***commands = commands_setup(file, &commands_amount);
  int **pipes = pipes_setup(commands_amount);
  fork_setup(commands, commands_amount, pipes);
  wait_for_children(commands_amount);

  cleanup(file, commands, commands_amount, pipes);

  return EXIT_SUCCESS;
}

FILE *handle_CmdLine_Args(int argc, char *argv[]) {

  // More then two arguments print usage message
  if (argc > 2) {
    fprintf(stderr, "usage: %s [file]\n", argv[0]);

    return NULL;
  }

  // if correct amount we should open the file provided in read mode
  if (argc == 2) {
    FILE *file = fopen(argv[1], "r");
    if (!file) {
      perror(argv[1]);
      return NULL;
    }

    return file;
  }

  return stdin;
}

char **parse_line(char *buffer) {
  int max_args = 30;
  char **args = malloc(sizeof(char *) * max_args);

  if (!args) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  // split the buffern at spaces tabs and new lines
  char *tokens = strtok(buffer, " \t\n");

  int i = 0;
  // Loop over and save duplicate each token to args
  while (tokens != NULL) {
    args[i] = strdup(tokens);
    if (args[i] == NULL) {
      perror(args[i]);
      exit(EXIT_FAILURE);
    }
    i++;
    tokens = strtok(NULL, " \t\n");
  }

  // put null at end for execvp
  args[i] = NULL;
  return args;
}

void free_args(char **args) {
  if (args == NULL)
    return;
  for (int i = 0; args[i] != NULL; i++) {
    free(args[i]);
  }
  free(args);
}

char ***commands_setup(FILE *file, int *commands_amount) {
  char line[MAX_LINE];
  int amount = 0;
  char ***commands = NULL;

  // read entire file or stdin
  while (fgets(line, MAX_LINE, file) != NULL) {

    // parse each line into array of arguments
    char **args = parse_line(line);
    if (!args)
      exit(EXIT_FAILURE);

    if (args[0] == NULL) {
      free_args(args);
      continue;
    }

    // resize commands to be able to store more args
    commands = realloc(commands, sizeof(char **) * (amount + 1));

    // check realloc
    if (!commands) {
      perror("realloc");
      exit(EXIT_FAILURE);
    }

    // save the args after reallocaing
    commands[amount] = args;
    amount++;
  }
  // updates the commands_amount by pointer
  *commands_amount = amount;

  return commands;
}

int **pipes_setup(int commands_amount) {
  int pipes_amount = commands_amount - 1;

  // Allocate memory for array of pipes
  int **pipes = malloc(sizeof(int *) * pipes_amount);

  if (!pipes) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < pipes_amount; i++) {
    // allocate memory for pipe read and write ends
    pipes[i] = malloc(sizeof(int) * 2);

    if (!pipes[i]) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    // run pipe on each FD and check that it works
    if (pipe(pipes[i]) == -1) {
      perror("pipe");
      exit(EXIT_FAILURE);
    }
  }

  return pipes;
}

void fork_setup(char ***commands, int commands_amount, int **pipes) {
  int pipes_amount = commands_amount - 1;
  // loop over all commands
  for (int i = 0; i < commands_amount; i++) {
    // fork for each command
    int pid = fork();
    if (pid == -1) {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    // if we are in the child
    // redirect the childs STDIN to the previous pipe
    // redirect the child STDOUT to the current pipe, skips the last command so
    // it actaully outputs something
    if (pid == 0) {
      if (i > 0)
        if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
          perror("dup2");
          exit(EXIT_FAILURE);
        }
      if (i < commands_amount - 1)
        if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
          perror("dup2");
          exit(EXIT_FAILURE);
        }

      // close all origional pipes since dup2 duplicates them
      for (int j = 0; j < pipes_amount; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }

      // execute the commands
      execvp(commands[i][0], commands[i]);
      perror(commands[i][0]);
      exit(EXIT_FAILURE);
    }
  }

  // close the parents copies of pipes
  for (int i = 0; i < pipes_amount; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }
}

void wait_for_children(int commands_amount) {
  int status;
  // waits for all children
  for (int i = 0; i < commands_amount; i++) {
    if (wait(&status) == -1) {
      perror("wait");
      exit(EXIT_FAILURE);
    }
    // checks exitcodes for all children
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      exit(EXIT_FAILURE);
    }
  }
}

void cleanup(FILE *file, char ***commands, int commands_amount, int **pipes) {
  // CLOSES AND CLEANUPS EVERYTHING

  if (file != stdin) {
    fclose(file);
  }
  for (int i = 0; i < commands_amount; i++) {
    free_args(commands[i]);
  }
  free(commands);

  for (int i = 0; i < commands_amount - 1; i++) {
    free(pipes[i]);
  }
  free(pipes);
}
