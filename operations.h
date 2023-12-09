#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H

#include "constants.h"
#include <stddef.h>

typedef struct args {
  unsigned int *event_id;
  unsigned int *delay;
  size_t xs[MAX_RESERVATION_SIZE];
  size_t ys[MAX_RESERVATION_SIZE];
  size_t *num_rows;
  size_t *num_columns;
  size_t *num_coords;
  int *fd_out;
  int *fd_in;
} Args;

/// Initializes the EMS state.
/// @param delay_ms State access delay in milliseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
void *ems_init(void *thread_delay_ms);

/// Destroys the EMS state.
void* ems_terminate();

void *thread_ems_create(void *thread_args);

/// Creates a new event with the given id and dimensions.
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols);

void *thread_ems_reserve(void *thread_args);

/// Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs,
                size_t *ys);

void *thread_ems_show(void *thread_args);

/// Prints the given event.
/// @param event_id Id of the event to print.
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(unsigned int event_id, int fd_out);

/// Prints all the events.
/// @return 0 if the events were printed successfully, 1 otherwise.
void *ems_list_events(void *thread_fd_out);

void *thread_ems_wait(void *thread_args);

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void ems_wait(unsigned int delay_ms);

#endif // EMS_OPERATIONS_H
