
#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"

#include "main.h"
#include "string.h"
#include "stdbool.h"

#include <sys/types.h>
#include <unistd.h>
#include <time.h>


typedef struct {
  char name[256];
  pid_t pid;
  char *command;
  time_t start_time;
  time_t end_time;
  int exit_code;
  int signal_value;
  bool is_alive;
} ProcessData;


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
  ProcessData processes_array[1024];

  char** commands = malloc(TIME_MAX * sizeof(char*));
  for (int i = 0; i < TIME_MAX; i++) {
    commands[i] = NULL;
  }

  // Loop de comandos
  while (true) {
    char** input = read_user_input();
    printf("%s\n", input[0]);

    // Llena la tabla de data con la información del proceso en el primer espacio vacio
    for (int i = 0; i < 1024; i++) {
      processes_array[i].pid = 0;
      strncpy(processes_array[i].name, input[1], sizeof(processes_array[i].name)-1);
      processes_array[i].start_time = time(NULL);
      processes_array[i].end_time = 0;
      processes_array[i].exit_code = -1;
      processes_array[i].signal_value = -1;
      processes_array[i].is_alive = 1;
      break;
    }

    // launch <executable> <arg1> <arg2> ... <argn>
    if (strcmp(input[0], "launch") == 0) {

      int process = fork();
      printf("Proceso hijo creado con PID: %d\n", process);
      if (process == 0) { 
        printf("Ejecutando: %s\n", input[1]);
        execvp(input[1], &input[1]);
        exit(EXIT_FAILURE);
       
      } else {
        // Proceso padre
        int index = pid_append(process, pid);
      }
    }

    // status
    else if (strcmp(input[0], "status") == 0) {
      int process = fork();
      if (process == 0) {

        for (int i = 0; i < 1024; i++) {
          if (processes_array[i].pid != 0) {
            printf("PID: %d\n", processes_array[i].pid);
            printf("Nombre: %s\n", processes_array[i].name);
            printf("Tiempo de ejecución: %ld segundos\n", processes_array[i].end_time - processes_array[i].start_time);
            printf("Exit code: %d\n", processes_array[i].exit_code);
            printf("Signal value: %d\n", processes_array[i].signal_value);
          } else {break;}
  
        }
        printf("PID: %d\n", getpid());
        sleep(3);
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

    // shutdown
    else if (strcmp(input[0], "shutdown") == 0) {
     
    }

    // emergency
    else if (strcmp(input[0], "emergency") == 0) {
      printf("¡Emergencia!\n DCControl finalizado.\n PID %d\n", getpid());
    }

    else {
      printf("Debug: Comando inválido\n");
    }

    free_user_input(input);
  }
}
