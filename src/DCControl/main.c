
#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"

#include "main.h"
#include "string.h"
#include "stdbool.h"

#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include <signal.h>
#include <sys/wait.h>


typedef struct {
  char name[256];
  pid_t pid;
  time_t start_time;
  time_t end_time;
  int exit_code;
  int signal_value;
  bool is_alive;
} ProcessData;

ProcessData processes_array[1024]; //movi esto para arriba para que sea global y el sigchld_handler pueda usarlo

// busca el primer espacio libre en el array, osea donde el pid == 0
int find_free_slot(ProcessData *processes_array) {
    for (int i = 0; i < 1024; i++) {
        if (processes_array[i].pid == 0) {
            return i;
        }
    }
    return -1; // la tabla esta llena
}

// esto es para que se actualicen los valores de exit code y el tiempo de ejecucion
//se activa cuando el proceso recibe la señal de SIGCHLD
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    //funciona con los hijos que ya terminaron de ejecutarse
    //espera a cualquier hijo (waitpid(-1,...)) y el WHOHANG sirve para evitar que se bloquee el padre si el hijo aun no termina
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // buscamos el proceso en el array
        for (int i = 0; i < 1024; i++) {
            if (processes_array[i].pid == pid) {
                processes_array[i].is_alive = false; //como terminó, ponemos que ya no  está alive
                processes_array[i].end_time = time(NULL); //guarda el tiempo en que terminó de ejecutarse

                //esto verificas si el codigo terminó normalmente, en ese caso guarda el codigo de salida
                if (WIFEXITED(status)) {
                    processes_array[i].exit_code = WEXITSTATUS(status);

                //esto se supone que guarda si el proceso fue terminado por una señal como SIGKILL
                //pero como aun no implementamos que los procesos terminen con esas señales nose si funciona
                } else if (WIFSIGNALED(status)) {
                    processes_array[i].signal_value = WTERMSIG(status);
                }
                break;
            }
        }
    }
}


int main(int argc, char const *argv[])
{
  set_buffer(); // No borrar

  //inicializamos el signal handler para actualizar el array cuando un proceso termina
  signal(SIGCHLD, sigchld_handler);

  //aqui inicializamos el array como "base"
  for (int i = 0; i < 1024; i++) {
    processes_array[i].pid = 0;
    processes_array[i].exit_code = -1;
    processes_array[i].signal_value = -1;
    processes_array[i].is_alive = false;
}


  // Loop de comandos
  while (true) {
    char** input = read_user_input();
    printf("%s\n", input[0]);

    // launch <executable> <arg1> <arg2> ... <argn>
    if (strcmp(input[0], "launch") == 0) {
      int process = fork();
      printf("Proceso hijo creado con PID: %d\n", process);

      if (process == 0) { 
        // este es el proceso hijo
        printf("(HIJO) Proceso hijo creado con PID: %d\n", process);
        printf("Ejecutando: %s\n", input[1]);
        execvp(input[1], &input[1]);
        perror("execv falló");
        exit(EXIT_FAILURE);

      } else if (process > 0) {
        // el proceso padre guarda la info del proceso en el array
        printf("(PADRE) Proceso hijo creado con PID: %d\n", process);
        int slot = find_free_slot(processes_array);
        if (slot == -1) {
          printf("Array de procesos lleno");
        }
        else {
          //guarda toda la info del proceso en el array
          processes_array[slot].pid = process;
          strncpy(processes_array[slot].name, input[1], sizeof(processes_array[slot].name)-1);
          processes_array[slot].start_time = time(NULL);
          processes_array[slot].end_time = 0;
          processes_array[slot].exit_code = -1;
          processes_array[slot].signal_value = -1;
          processes_array[slot].is_alive = true;
        }
      }
      else {
          perror("fork falló");
        }
    }

    // status
    else if (strcmp(input[0], "status") == 0) {
      int process = fork();

      //aqui CREO que no sería necesario hacer un fork y hacer el manejo de padre e hijo,
      //pq al final solo necesitamos imprimir el estado de los procesos asi que no hay que
      //esperar a que nada se ejecute de fondo, pero igual lo dejaría asi porsiacaso y
      //porque por ahora parece que funciona xd

      if (process == 0) {
        //primero imprimimos los nombres de cada una de las cosas que hay que mostrar con el status
        printf("%-8s %-10s %-20s %-12s %-13s\n", 
              "PID", "Nombre", "Tiempo de ejecución", "Exit code", "Signal Value");

        for (int i = 0; i < 1024; i++) {

            //esto es para ver el tiempo de ejecucion que lleva un proceso que aun no ha terminado
            if (processes_array[i].pid != 0) {
              long runtime;
              if (processes_array[i].is_alive) {
                //si es que esta alive entonces calcula con el tiempo actual - tiempo inicio
                  runtime = time(NULL) - processes_array[i].start_time;
              } else {
                //si ya terminó calcula el tiempo normalmente, con su end time - start time
                  runtime = processes_array[i].end_time - processes_array[i].start_time;
              }
              
              //esos % son separadores para que esté alineado con las categorias, en realidad no son importantes
              //pero es para que se vean bien, separandolos con un \t tambien sirve pero se ve movido
              printf("%-8d %-10s %-20ld %-12d %-13d\n",
                    processes_array[i].pid,
                    processes_array[i].name,
                    runtime,
                    processes_array[i].exit_code,
                    processes_array[i].signal_value);
          } else {
            //si el pid es 0 entonces ya recorrimos todos los procesos o no hay
              break;
          }
        }
        exit(EXIT_SUCCESS);
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
