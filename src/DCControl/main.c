
#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"

#include "main.h"
#include <string.h>
#include <stdbool.h>

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

// variable global del parámetro <time_max>
int time_max = -1;

ProcessData processes_array[1024]; //movi esto para arriba para que sea global y el sigchld_handler pueda usarlo

// globales para el modo shutdown
bool shutdown_mode = false;
time_t shutdown_start_time = 0;

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

  // <time_max>
  if (argc > 1) {
    time_max = atoi(argv[1]);
    // valor inválido, tiempo ilimitado por defecto
    if (time_max <= 0) {
      time_max = -1; 
    }
  }

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
    // MODO SHUTDOWN: si hay modo shutdown, verifica si han pasado 10 segundos
    if (shutdown_mode) {
      if (difftime(time(NULL), shutdown_start_time) >= 10) {
        // envia SIGKILL a procesos vivos
        for (int i = 0; i < 1024; i++) {
          if (processes_array[i].is_alive) {
            kill(processes_array[i].pid, SIGKILL);
          }
        }
        // imprimir estadísticas y terminar
        printf("DDControl finalizado.\n");
        printf("%-8s %-10s %-20s %-12s %-13s\n", "PID", "Nombre", "Tiempo de ejecución", "Exit code", "Signal Value");
        for (int i = 0; i < 1024; i++) {
          if (processes_array[i].pid != 0) {
            printf("%-8d %-10s %-20ld %-12d %-13d\n",
                  processes_array[i].pid,
                  processes_array[i].name,
                  processes_array[i].end_time - processes_array[i].start_time,
                  processes_array[i].exit_code,
                  processes_array[i].signal_value);
          }
        }
        exit(EXIT_SUCCESS);
      }
    }
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

          // si hay límite de tiempo, crear watcher para terminar el proceso si excede time_max
          if (time_max > 0) {
            pid_t child_pid = process;
            int child_slot = slot;
            // crear un proceso watcher
            pid_t watcher = fork();
            if (watcher == 0) {
              // proceso watcher
              sleep(time_max);
              // verifica si el proceso sigue vivo
              if (kill(child_pid, 0) == 0) {
                printf("Proceso %s con PID %d excedió el tiempo máximo (%d s)\n", processes_array[child_slot].name, child_pid, time_max);
                kill(child_pid, SIGKILL);
              }
              exit(0);
            }
            // el padre no hace nada con watcher
          }
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
      // si se está en modo shutdown, se ignoran los aborts
      if (shutdown_mode) {
        printf("abort ignorado, shotdown en proceso.\n");
      } else {

        //convertimos el time ingresado a un int
        int seconds = atoi(input[1]);

        pid_t abort_pid = fork();
        
        if (abort_pid == 0) {

          //esto es para ver los procesos que estaban vivos al momento de ejecutar el comando abort
          pid_t to_kill[1024];  //aqui guardamos los id de los procesos a abortar
          int cantidad_alive = 0;

          //verificamos si hay procesos alive
          for (int i = 0; i < 1024; i++) {
              if (processes_array[i].pid != 0 && processes_array[i].is_alive) {
                //guardamos el pid del proceso en process_array y aumentamos cantidad_alive en 1
                to_kill[cantidad_alive++] = processes_array[i].pid;
              }
          }
          
          //si no hay ningun proceso alive entonces imprime que no hay procesos en ejecucion
          if (cantidad_alive == 0) {
              printf("No hay procesos en ejecución. Abort no se puede ejecutar.\n");
          } else {
            // esperamos los segundos que se indicaron en <time>
            sleep(seconds);

            // imprimimos la "cabecera" para mostrar los procesos que luego del sleep siguen ejecutandose
            printf("Abort cumplido.\n");
            printf("%-8s %-10s %-20s %-12s %-13s\n", 
                  "PID", "Nombre", "Tiempo de ejecución", "Exit code", "Signal Value");

            for (int j = 0; j < cantidad_alive; j++) {
              pid_t pid = to_kill[j];  //iteramos sobre la lista de los procesos a abortar

              //buscamos en el process_array el proceso con el pid que necesitamos
              for (int i = 0; i < 1024; i++) {
                //aqui verificamos que si encontramos el proceso con ese pid que aún siga vivo

                if (processes_array[i].pid == pid && processes_array[i].is_alive) {
                  //calculamos el tiempo de ejecución actual, ya que cmo no ha terminado no tiene un end_time
                  long runtime = time(NULL) - processes_array[i].start_time;

                  //imprimimos la información del proceso
                  printf("%-8d %-10s %-20ld %-12d %-13d\n",
                          processes_array[i].pid,
                          processes_array[i].name,
                          runtime,
                          processes_array[i].exit_code,
                          processes_array[i].signal_value);

                  // enviamos un sigterm para abortar el proceso, si hubo error retorna -1 (nse en que casos podria dar error)
                  if (kill(processes_array[i].pid, SIGTERM) == -1) {
                      perror("Error con el SIGTERM");
                  }
                  break;
                }
              }
            }
          }
          exit(EXIT_SUCCESS);
        } else if (abort_pid > 0) {
          // proceso padre no hace nada
        } 
        else {
          perror("fork falló");
      }
    }
    }

    // shutdown
    else if (strcmp(input[0], "shutdown") == 0) {
      // verificar si hay procesos vivos
      bool any_alive = false;
      for (int i = 0; i < 1024; i++) {
        if (processes_array[i].is_alive) {
          any_alive = true;
          break;
        }
      }

      if (!any_alive) {
        // no hay procesos vivos, imprimir estadísticas y terminar inmediatamente
        printf("DDControl finalizado.\n");
        printf("%-8s %-10s %-20s %-12s %-13s\n", "PID", "Nombre", "Tiempo de ejecución", "Exit code", "Signal Value");
        for (int i = 0; i < 1024; i++) {
          if (processes_array[i].pid != 0) {
            printf("%-8d %-10s %-20ld %-12d %-13d\n",
                  processes_array[i].pid,
                  processes_array[i].name,
                  processes_array[i].end_time - processes_array[i].start_time,
                  processes_array[i].exit_code,
                  processes_array[i].signal_value);
          }
        }
        exit(EXIT_SUCCESS);
      } else {
        // hay procesos vivos, enviar SIGINT a todos los que se esten ejecutando
        for (int i = 0; i < 1024; i++) {
          if (processes_array[i].is_alive) {
            kill(processes_array[i].pid, SIGINT);
          }
        }
        // activar modo shutdown y guardar tiempo de inicio
        shutdown_mode = true;
        shutdown_start_time = time(NULL);
        // debug: printf("shutdown iniciado. El programa aceptará comandos por 10 segundos antes de finalizar.\n");
        // el resto de la lógica se maneja en el loop principal
      }
    }

    // emergency
    else if (strcmp(input[0], "emergency") == 0) {
      // Enviar SIGKILL a todos los procesos vivos y actualizar end_time si siguen vivos
      for (int i = 0; i < 1024; i++) {
        if (processes_array[i].is_alive) {
          kill(processes_array[i].pid, SIGKILL);
          processes_array[i].end_time = time(NULL); // Actualiza end_time manualmente
          processes_array[i].is_alive = false; // Marca como no vivo
        }
      }
      printf("¡Emergencia!\nDCControl finalizado.\n");
      printf("%-8s %-10s %-20s %-12s %-13s\n", "PID", "Nombre", "Tiempo de ejecución", "Exit code", "Signal Value");
      for (int i = 0; i < 1024; i++) {
        if (processes_array[i].pid != 0) {
          long runtime = processes_array[i].end_time > 0 ? (processes_array[i].end_time - processes_array[i].start_time) : (time(NULL) - processes_array[i].start_time);
          printf("%-8d %-10s %-20ld %-12d %-13d\n",
                processes_array[i].pid,
                processes_array[i].name,
                runtime,
                processes_array[i].exit_code,
                processes_array[i].signal_value);
        }
      }
      exit(EXIT_SUCCESS);
    }

    else {
      printf("Debug: Comando inválido\n");
    }

    free_user_input(input);
  }
}
