#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "aux.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"

int create_output_file(char *filename, char *dirname) {
  char final[1025];
  char output_name[512];
  char input_name[512];
  strcpy(input_name, filename);
  char *ptr = strstr(input_name, INPUT_EXTENSION);
  memcpy(ptr, OUTPUT_EXTENSION, 5);
  strcpy(output_name, dirname);
  sprintf(final, "%s/%s", dirname, input_name);
  return open(final, O_CREAT | O_RDWR | O_TRUNC, 0666);
}

int execute_file(int fd_in, int fd_out, unsigned state_access_delay_ms) {

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  while (1) {
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

    fflush(stdout);

    switch (get_next(fd_in)) {
    case CMD_CREATE:
      if (parse_create(fd_in, &event_id, &num_rows, &num_columns) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_create(event_id, num_rows, num_columns)) {
        fprintf(stderr, "Failed to create event\n");
      }

      break;

    case CMD_RESERVE:
      num_coords =
          parse_reserve(fd_in, MAX_RESERVATION_SIZE, &event_id, xs, ys);

      if (num_coords == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_reserve(event_id, num_coords, xs, ys)) {
        fprintf(stderr, "Failed to reserve seats\n");
      }

      break;

    case CMD_SHOW:
      if (parse_show(fd_in, &event_id) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_show(event_id, fd_out)) {
        fprintf(stderr, "Failed to show event\n");
      }

      break;

    case CMD_LIST_EVENTS:
      if (ems_list_events(fd_out)) {
        fprintf(stderr, "Failed to list events\n");
      }

      break;

    case CMD_WAIT:
      if (parse_wait(fd_in, &delay, NULL) ==
          -1) { // thread_id is not implemented
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (delay > 0) {
        printf("Waiting...\n");
        ems_wait(delay);
      }

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
