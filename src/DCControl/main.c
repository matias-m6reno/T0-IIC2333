
#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"

#include "main.h"
#include "string.h"
#include "stdbool.h"

#include <unistd.h>

// PID array
int pid[1024];

// Agrega procesos a PID array
int pid_append(int pid_value, int *pid) {
  for (int i = 1; i < 1024; i++) {
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

  // Crear array de procesos
  char** commands = malloc(TIME_MAX * sizeof(char*));
  for (int i = 0; i < TIME_MAX; i++) {
    commands[i] = NULL;
  }

  // Loop de comandos
  while (true) {
    char** input = read_user_input();
    printf("%s\n", input[0]);
    
    // launch <executable> <arg1> <arg2> ... <argn>
    if (strcmp(input[0], "launch") == 0) {
      // Proceso hijo
      int launch_process = fork();
      printf("Proceso hijo creado con PID: %d\n", launch_process);
      if (launch_process == 0) { 
        printf("Ejecutando: %s\n", input[1]);
        execvp(input[1], &input[1]);
        exit(EXIT_FAILURE);
       
      } else {
        // Proceso padre
        int index = pid_append(launch_process, pid);
      }
    }

    // status
    else if (strcmp(input[0], "status") == 0) {
      int status_process = fork();
      if (status_process == 0) {
        // Proceso hijo
        printf("Estado del proceso: %d\n", getpid());
        sleep(5);
        printf("Hola\n");
        exit(EXIT_SUCCESS);
        
      } else {
        // Proceso padre
        int index = pid_append(status_process, pid);
      }
     
    }

    // abort <time>
    else if (strcmp(input[0], "abort") == 0) {
      if (input[1] != NULL) {
        int time = atoi(input[1]);
        printf("Abortando proceso en %d segundos...\n", time);
        sleep(time);
      } else {
        printf("Uso: abort <time>\n");
      }
    }

    // status
    else if (strcmp(input[0], "status") == 0) {
     
    }

    // status
    else if (strcmp(input[0], "status") == 0) {
     
    }

    // status
    else if (strcmp(input[0], "status") == 0) {
     
    }
    

    else {
      printf("Debug: Comando inválido\n");
    }

    free_user_input(input);
  }
}
