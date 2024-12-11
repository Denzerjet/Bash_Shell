In short, shell.l is the lexicographical parser which takes in input from the user and assembles tokens from it according a grammar which I implemented.

Shell.y is the yacc (yet another compiler compiler) which uses those tokens and follows a goal to build a command chain.

command.c details the operations for the command struct (defined in command.h) which holds an array of structs of single_command.

Example: a command struct might represent *ls -al | grep command > out.txt*

  a single_command struct might represent the *ls - al* or *grep command > out.txt* parts of that.
  
Environment variable expansion is performed in shell.l, and wildcarding in single_command.c.

There are other features that are part of standard bash like subshell, backgrounding processes, etc...


Overall:

shell.l -> shell.y -> command.c:execute_command()

shell.c is the main entry point to the program.
