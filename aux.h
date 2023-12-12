#ifndef AUX_H
#define AUX_H

#include <pthread.h>

typedef struct filein {
  int fd;
  pthread_mutex_t mutex;
} Filein;

typedef struct args {
  int filein;
  int fd_out;
  int thread_id;
  int max_threads;
} Args;

/// Creates a output file for the given input file
/// @param filename is the name of the input file
/// @return the file descriptor of the output file
int create_output_file(char *filename, char *dirname);

void *run_thread(void *thread_args);

/// Executes the commands on an input file and executes the commands
/// @param filein descriptor of the input file
/// @param fd_out File descriptor of the output file
/// @return 0 if suceeds
int execute_file(char *filein, int fd_out, unsigned int state_access_delay_ms,
                 int max_threads);

/// Write that handles all the arguments
/// @param fd file descriptor of the output file
/// @param string that will be written
void mywrite(int fd, char *string);

#endif // AUX_H
