#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aux.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"

int check_line(int thread_id, int line, int max_threads) {
  return line % max_threads == thread_id;
}

void *run_thread(void *thread_args) {
  Args *args = (Args *)thread_args;
  int line = 0;
  while (1) {
    unsigned int event_id, delay, thread_id;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    fflush(stdout);
    switch (get_next(args->fd_in, &line)) {
    case CMD_CREATE:
      if (parse_create(args->fd_in, &event_id, &num_rows, &num_columns) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (!check_line(args->thread_id, line, args->max_threads)) {
        break;
      }

      if (ems_create(event_id, num_rows, num_columns)) {
        fprintf(stderr, "Failed to create event\n");
      }
      break;
    case CMD_RESERVE:
      num_coords =
          parse_reserve(args->fd_in, MAX_RESERVATION_SIZE, &event_id, xs, ys);
      if (num_coords == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (!check_line(args->thread_id, line, args->max_threads)) {
        break;
      }

      if (ems_reserve(event_id, num_coords, xs, ys)) {
        fprintf(stderr, "Failed to reserve seats\n");
      }
      break;
    case CMD_SHOW:
      if (parse_show(args->fd_in, &event_id) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (!check_line(args->thread_id, line, args->max_threads)) {
        break;
      }

      if (ems_show(event_id, args->fd_out)) {
        fprintf(stderr, "Failed to show event\n");
      }
      break;
    case CMD_LIST_EVENTS:
      if (!check_line(args->thread_id, line, args->max_threads)) {
        break;
      }
      if (ems_list_events(args->fd_out)) {
        fprintf(stderr, "Failed to list events\n");
      }

      break;
    case CMD_WAIT:
      if (parse_wait(args->fd_in, &delay, &thread_id) ==
          -1) { 
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }
      if (delay > 0 && ((int) thread_id == args->thread_id + 1 || thread_id == 0)) {
        printf("Waiting...\n");
        ems_wait(delay);
      }
      break;
    case CMD_INVALID:
      if (!check_line(args->thread_id, line, args->max_threads)) {
        break;
      }

      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;
    case CMD_HELP:
      if (!check_line(args->thread_id, line, args->max_threads)) {
        break;
      }

      printf("Available commands:\n"
             "  CREATE <event_id> <num_rows> <num_columns>\n"
             "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
             "  SHOW <event_id>\n"
             "  LIST\n"
             "  WAIT <delay_ms> [thread_id]\n" 
             "  BARRIER\n"
             "  HELP\n");
      break;
    case CMD_BARRIER:
      pthread_exit(BARRIER);
    case CMD_EMPTY:

      break;
    case EOC:
      close(args->fd_in);
      pthread_exit(SUCESS);
    }
  }
  close(args->fd_in);
  pthread_exit(SUCESS);
}

int create_output_file(char *filename, char *dirname) {
  char final[1024];
  char input_name[512];
  strcpy(input_name, filename);
  char *ptr = strstr(input_name, INPUT_EXTENSION);
  memcpy(ptr, OUTPUT_EXTENSION, 5);
  sprintf(final, "%s/%s", dirname, input_name);
  return open(final, O_CREAT | O_RDWR | O_TRUNC, 0666);
}

int execute_file(char *filein, int fd_out, unsigned int state_access_delay_ms,
                 int max_threads) {
  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  pthread_t *threads = malloc((unsigned long)max_threads * sizeof(pthread_t));
  Args *args_list = malloc((unsigned long)max_threads * sizeof(Args));
  for (int i = 0; i < max_threads; i++) {
    int fd_in = open(filein, O_RDONLY);
    args_list[i].fd_in = fd_in;
    args_list[i].fd_out = fd_out;
    args_list[i].max_threads = max_threads;
    args_list[i].thread_id = i;
    pthread_create(&threads[i], NULL, run_thread, (void *)&args_list[i]);
  }

  void *r_value;
  int is_barrier = 0;
  while (1) {
    for (int i = 0; i < max_threads; i++) {
      pthread_join(threads[i], &r_value);
      if (r_value == BARRIER) {
        is_barrier = 1;
      }
    }
    if (is_barrier) {
      is_barrier = 0;
      for (int i = 0; i < max_threads; i++) {
        pthread_create(&threads[i], NULL, run_thread, (void *)&args_list[i]);
      }
    } else {
      break;
    }
  } free(args_list);

  ems_terminate();
  free(args_list);
  free(threads);
  return 0;
}
void mywrite(int fd, char *string) {
  size_t len = strlen(string);
  if (write(fd, string, len) < 0) {
    fprintf(stderr, "write error: %s\n", strerror(errno));
  }
}