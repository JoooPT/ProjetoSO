#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "aux.h"
#include "eventlist.h"
#include "operations.h"
#include "parser.h"

static struct EventList *event_list = NULL;
static unsigned int state_access_delay_ms = 0;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event *get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory
/// resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int *get_seat_with_delay(struct Event *event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL); // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event *event, size_t row, size_t col) {
  return (row - 1) * event->cols + col - 1;
}

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(event_list);
  return 0;
}

void *thread_ems_create(void *thread_args) {
  Args *args = (Args *)thread_args;
  if (parse_create(*(args->fd_in->fd), args->event_id, args->num_rows,
                   args->num_columns) != 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
    pthread_mutex_unlock(&(args->fd_in->mutex));
    pthread_exit((void *)1);
  }
  pthread_mutex_unlock(&(args->fd_in->mutex));
  if (ems_create(*(args->event_id), *(args->num_rows), *(args->num_columns))) {
    fprintf(stderr, "Failed to create event\n");
  }
  pthread_exit(NULL);
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  pthread_mutex_lock(&event_list->mutex);

  if (get_event_with_delay(event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_mutex_unlock(&event_list->mutex);
    return 1;
  }

  struct Event *event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_mutex_unlock(&event_list->mutex);
    return 1;
  }

  pthread_mutex_init(&event->mutex, NULL);
  pthread_mutex_lock(&event->mutex);
  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    pthread_mutex_unlock(&event->mutex);
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    pthread_mutex_unlock(&event->mutex);
    free(event);
    return 1;
  }
  pthread_mutex_unlock(&event->mutex);
  pthread_mutex_unlock(&event_list->mutex);
  return 0;
}

void *thread_ems_reserve(void *thread_args) {
  Args *t_args = (Args *)thread_args;
  Args *args = malloc(sizeof(Args));
  *args = *t_args;
  *(args->num_coords) = parse_reserve(*(args->fd_in->fd), MAX_RESERVATION_SIZE,
                                      args->event_id, args->xs, args->ys);

  pthread_mutex_unlock(&(args->fd_in->mutex));
  if (*(args->num_coords) == 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
    free(args);
    pthread_exit((void *)1);
  }

  if (ems_reserve(*(args->event_id), *(args->num_coords), args->xs, args->ys)) {
    fprintf(stderr, "Failed to reserve seats\n");
  }
  free(args);
  pthread_exit(NULL);
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs,
                size_t *ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  pthread_mutex_lock(&event_list->mutex);

  struct Event *event = get_event_with_delay(event_id);
  pthread_mutex_lock(&event->mutex);
  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    pthread_mutex_unlock(&event->mutex);
    pthread_mutex_unlock(&event_list->mutex);
    return 1;
  }

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
    }
    pthread_mutex_unlock(&event->mutex);
    pthread_mutex_unlock(&event_list->mutex);
    return 1;
  }
  pthread_mutex_unlock(&event->mutex);
  pthread_mutex_unlock(&event_list->mutex);
  return 0;
}

void *thread_ems_show(void *thread_args) {
  Args *args = (Args *)thread_args;
  if (parse_show(*(args->fd_in->fd), args->event_id) != 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
    pthread_mutex_unlock(&(args->fd_in->mutex));
    pthread_exit((void *)1);
  }
  pthread_mutex_unlock(&(args->fd_in->mutex));
  if (ems_show(*(args->event_id), *(args->fd_out))) {
    fprintf(stderr, "Failed to show event\n");
  }
  pthread_exit(NULL);
}

int ems_show(unsigned int event_id, int fd_out) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  pthread_mutex_lock(&event_list->mutex);
  struct Event *event = get_event_with_delay(event_id);
  pthread_mutex_lock(&event->mutex);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    pthread_mutex_unlock(&event->mutex);
    pthread_mutex_unlock(&event_list->mutex);
    return 1;
  }

  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int *seat = get_seat_with_delay(event, seat_index(event, i, j));
      char seatchar[64];
      sprintf(seatchar, "%u", *seat);
      mywrite(fd_out, seatchar);
      // printf("%u", *seat);

      if (j < event->cols) {
        mywrite(fd_out, " ");
        // printf(" ");
      }
    }
    mywrite(fd_out, "\n");
    // printf("\n");
  }
  pthread_mutex_unlock(&event->mutex);
  pthread_mutex_unlock(&event_list->mutex);
  return 0;
}

void *ems_list_events(void *thread_fd_out) {
  int *fd_out = (int *)thread_fd_out;
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    pthread_exit((void *)1);
  }
  pthread_mutex_lock(&event_list->mutex);
  if (event_list->head == NULL) {
    mywrite(*(fd_out), "No events\n");
    // printf("No events\n");
    pthread_mutex_unlock(&event_list->mutex);
    pthread_exit((void *)0);
  }

  struct ListNode *current = event_list->head;
  while (current != NULL) {
    pthread_mutex_lock(&(current->event)->mutex);
    mywrite(*fd_out, "Event: ");
    char id[64];
    sprintf(id, "%u", (current->event)->id);
    mywrite(*fd_out, strcat(id, "\n"));
    pthread_mutex_unlock(&(current->event)->mutex);

    // printf("Event: ");
    // printf("%u\n", (current->event)->id);
    current = current->next;
  }

  pthread_mutex_unlock(&event_list->mutex);
  pthread_exit((void *)0);
}

void *thread_ems_wait(void *thread_args) {
  Args *args = (Args *)thread_args;
  if (parse_wait(*(args->fd_in->fd), args->delay, NULL) ==
      -1) { // thread_id is not implemented
    fprintf(stderr, "Invalid command. See HELP for usage\n");
    pthread_mutex_unlock(&(args->fd_in->mutex));
    pthread_exit((void *)1);
  }
  pthread_mutex_unlock(&(args->fd_in->mutex));

  if (*(args->delay) > 0) {
    printf("Waiting...\n");
    ems_wait(*(args->delay));
  }
  pthread_exit(NULL);
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
