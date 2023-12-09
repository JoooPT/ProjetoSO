#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "aux.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  int max_procs = MAX_PROC;
  int max_threads = MAX_THREADS;

  if (argc > 4) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (argc > 3) {
    max_threads = atoi(argv[3]);
    if (max_threads > INT_MAX) {
      fprintf(stderr, "Invalid max threads value or value too large\n");
      return 1;
    }
  }

  if (argc > 2) {
    max_procs = atoi(argv[2]);

    if (max_procs > INT_MAX) {
      fprintf(stderr, "Invalid max process value or value too large\n");
      return 1;
    }
  }

  if (argc > 1) {
    DIR *jobs_dir = opendir(argv[1]);
    struct dirent *fileptr;
    int active_procs = 0;
    pid_t pid;
    pid_t child_pid;
    int status;
    while (jobs_dir) {
      if ((fileptr = readdir(jobs_dir)) != NULL) {
        if (strstr(fileptr->d_name, INPUT_EXTENSION) != NULL) {
          if (active_procs == max_procs) {
            child_pid = wait(&status);
            printf("Process %d terminated with status %d\n", child_pid, status);
            active_procs--;
          }
          active_procs++;
          pid = fork();
          if (pid == 0) {
            char filein[1024];
            sprintf(filein, "%s/%s", argv[1], fileptr->d_name);
            int fd_in = open(filein, O_RDONLY);
            int fd_out = create_output_file(fileptr->d_name, argv[1]);

            execute_file(fd_in, fd_out, state_access_delay_ms, max_threads);

            close(fd_in);
            close(fd_out);
            exit(0);
          }
        }
      } else
        break;
    }
    while (active_procs > 0) {
      child_pid = wait(&status);
      printf("Process %d terminated with status %d\n", child_pid, status);
      active_procs--;
    }
    closedir(jobs_dir);
  }
}
