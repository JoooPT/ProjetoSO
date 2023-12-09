#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "aux.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "threads.h"

#define thread_index(current, max) (current % max)

int create_output_file(char *filename, char *dirname) {
  char final[1024];
  char input_name[512];
  strcpy(input_name, filename);
  char *ptr = strstr(input_name, INPUT_EXTENSION);
  memcpy(ptr, OUTPUT_EXTENSION, 5);
  sprintf(final, "%s/%s", dirname, input_name);
  return open(final, O_CREAT | O_RDWR | O_TRUNC, 0666);
}

int execute_file(int fd_in, int fd_out, unsigned state_access_delay_ms,
                 int max_threads) {

  pthread_t *tid = malloc(max_threads * sizeof(pthread_t));
  int num_threads = 0;
  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  while (1) {
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    Args args;
    args.event_id = &event_id;
    args.delay = &delay;
    args.num_rows = &num_rows;
    args.num_columns = &num_columns;
    args.num_coords = &num_coords;
    args.fd_out = &fd_out;
    args.fd_in = &fd_in;
    fflush(stdout);

    if (num_threads >= max_threads) {
      pthread_join(tid[thread_index( num_threads, max_threads)], NULL);
    }

    switch (get_next(fd_in)) {
    case CMD_CREATE:
      pthread_create(&tid[thread_index( num_threads, max_threads)], NULL, thread_ems_create, (void *)&args);
      num_threads++;

      break;

    case CMD_RESERVE:
      pthread_create(&tid[thread_index( num_threads, max_threads)], NULL, thread_ems_reserve,
                     (void *)&args);
      num_threads++;
      break;

    case CMD_SHOW:
      pthread_create(&tid[thread_index( num_threads, max_threads)], NULL, thread_ems_show, (void *)&args);
      num_threads++;

      break;

    case CMD_LIST_EVENTS:
      if (pthread_create(&tid[thread_index( num_threads, max_threads)], NULL, ems_list_events,
                         (void *)(args.fd_out))) {
        fprintf(stderr, "Failed to list events\n");
      }
      num_threads++;

      break;

    case CMD_WAIT:
      pthread_create(&tid[thread_index( num_threads, max_threads)], NULL, thread_ems_wait, (void *)&args);
      num_threads++;

      break;

    case CMD_INVALID:
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;

    case CMD_HELP:
      printf("Available commands:\n"
             "  CREATE <event_id> <num_rows> <num_columns>\n"
             "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
             "  SHOW <event_id>\n"
             "  LIST\n"
             "  WAIT <delay_ms> [thread_id]\n" // thread_id is not implemented
             "  BARRIER\n"                     // Not implemented
             "  HELP\n");

      break;

    case CMD_BARRIER: // Not implemented
    case CMD_EMPTY:
      break;

    case EOC:
      ems_terminate();
      return 0;
    }
  }
}

void mywrite(int fd, char *string) {
  size_t len = strlen(string);
  if (write(fd, string, len) < 0) {
    fprintf(stderr, "write error: %s\n", strerror(errno));
  }
}
