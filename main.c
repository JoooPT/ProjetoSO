#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "aux.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  
  if (argc > 3) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (argc > 2) {
    DIR *jobs_dir = opendir(argv[1]);
    struct dirent *fileptr;
    int active_procs = 0;
    int max_procs = atoi(argv[2]);
    pid_t pid;
    while (jobs_dir) {
      if ((fileptr = readdir(jobs_dir)) != NULL) {
        if (strstr(fileptr->d_name, INPUT_EXTENSION) != NULL) {
          int status;
          if (active_procs == max_procs) {
            wait(&status);
            active_procs--;
          }
          active_procs++;
          pid = fork();
          if (pid == 0) {
            char filein[1024];
            sprintf(filein,"%s/%s", argv[1],fileptr->d_name);
            int fd_in = open(filein, O_RDONLY);
            int fd_out = create_output_file(fileptr->d_name, argv[1]);
            
            execute_file(fd_in, fd_out, state_access_delay_ms);

            close(fd_in);
            close(fd_out);
            exit(0);
          }
        }
      } 
      else break;
    }
    closedir(jobs_dir);
  }

}
