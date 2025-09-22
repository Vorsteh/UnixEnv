    32	
    33	  FILE *file = handleCmdLineArgs(argc, argv);
    34	
    35	  if (!file)
    36	    return EXIT_FAILURE;
    37	
    38	  char line[MAX_LINE];
--
    66	
    67	    free_args(args);
    68	  }
    69	
    70	  return EXIT_SUCCESS;
    71	}
    72	
    73	FILE *handleCmdLineArgs(int argc, char *argv[]) {
    74	  if (argc > 2) {
    75	    fprintf(stderr, "Usage: %s [file]\n", argv[0]);
    76	
    77	    return NULL;
    78	  }
    79	
--
    81	    FILE *file = fopen(argv[1], "r");
    82	
    83	    if (!file) {
    84	      perror("fopen");
    85	      return NULL;
    86	    }
    87	
    88	    return file;
    89	  }
    90	
    91	  return stdin;
    92	}
    93	
--
    96	  char **args = malloc(sizeof(char *) * max_args);
    97	
    98	  if (!args) {
    99	    fprintf(stderr, "Couldnt allocate memory for arguments");
   100	    return NULL;
   101	  }
   102	
--
   109	    tokens = strtok(NULL, " \t\n");
   110	  }
   111	
   112	  args[i] = NULL;
   113	  return args;
   114	}
   115	
   116	void free_args(char **args) {
   117	  if (args == NULL)
   118	    return;
   119	  for (int i = 0; args[i] != NULL; i++) {
   120	    free(args[i]);
