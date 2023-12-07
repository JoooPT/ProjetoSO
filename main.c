#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aux.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc > 1) {
    DIR *jobs_dir = opendir(argv[1]);
    struct dirent *fileptr;
    while (jobs_dir) {
      if ((fileptr = readdir(jobs_dir)) != NULL) {
        if (strstr(fileptr->d_name, INPUT_EXTENSION) != NULL) {
          char temp[512];
          char filein[1025];
          strcpy(temp,argv[1]);
          sprintf(filein,"%s/%s",temp,fileptr->d_name );
          int fd_in = open(filein, O_RDONLY);
          int fd_out = create_output_file(fileptr->d_name, argv[1]);
          
          execute_file(fd_in, fd_out, state_access_delay_ms);

          close(fd_in);
          close(fd_out);
        }
      } else {
        break;
      }
    }
    closedir(jobs_dir);
  }

  if (argc > 2) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }
}
