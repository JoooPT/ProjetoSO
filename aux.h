#ifndef AUX_H
#define AUX_H

/// Creates a output file for the given input file
/// @param filename is the name of the input file
/// @return the file descriptor of the output file
int create_output_file(char *filename, char *dirname);

/// Executes the commands on an input file and executes the commands
/// @param fd_in File descriptor of the input file
/// @param fd_out File descriptor of the output file
/// @return 0 if suceeds
int execute_file(int fd_in, int fd_out, unsigned int state_access_delay_ms);

/// Write that handles all the arguments
/// @param fd file descriptor of the output file
/// @param string that will be written
void mywrite(int fd, char *string);

#endif // AUX_H
