
#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"

#include "main.h"
#include "string.h"
#include "stdbool.h"

// PID array
int pid[TIME_MAX];


// Agrega procesos a PID array
int pid_append(int pid_value, int *pid) {
  for (int i = 1; i < TIME_MAX; i++) {
    if (pid[i] == 0) {
      pid[i] = pid_value;
      return i;
    }
  }
  return 0;
}


int main(int argc, char const *argv[])
{
  set_buffer(); // No borrar

  char** input = read_user_input();
  printf("%s\n", input[0]);
  free_user_input(input);

  // Loop de comandos
  while (true) {
    // launch <executable> <arg1> <arg2> ... <argn>
    if (strcmp(input[0], "launch") == 0) {

    }

    // status
    else if (strcmp(input[0], "status") == 0) {
            
    }

    else {
      printf("Debug: Comando inválido\n");
    }
  }
  


}
