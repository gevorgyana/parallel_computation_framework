#include "common.h"
#include "funcs.h"

#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

void Report(const int* results)
{
  // calculate minimum of the return
  // values that we have
  unsigned int imin_result;
  for (int i = 0; i < n; ++i) {
    if (i == 0)
    {
      imin_result = results[i];
      continue;
    }
    imin_result = (imin_result < results[i] ? imin_result : results[i]);
  }

  printf("The answer: %u\n\r", imin_result);
}

void StopChildProcesses(const int* children_pids)
{
  for (int j = 0; j < n; ++j) {
    kill(children_pids[j], SIGTERM);
  }
}

void RefreshReadFds(fd_set* reads,
                         const int* my_fds,
                         const int* results)
{
  // refresh info about children processes
  FD_ZERO(reads);
  for (int i = 0; i < n; ++i) {

    // have already read response
    if (results[i] >= 0) {
      continue;
    }

    FD_SET(my_fds[i], reads);
  }
}


void RestoreTerminalSettings()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &original_settings);
}

void PrepareTerminal()
{
  // store previous settings
  tcgetattr(STDIN_FILENO, &original_settings);

  // disable echoing and line-by-line reading
  struct termios temp_termios;
  temp_termios.c_lflag &= (~ICANON);
  temp_termios.c_cc[VMIN] = 1;
  temp_termios.c_cc[VTIME] = 0;
  temp_termios.c_lflag &= (~ECHO);

  /**
   * By default, newline character is converted to '\n\r'
   * In non-canonical mode, this is no longer desirable
   */
  temp_termios.c_oflag &= (~OPOST);

  // apply new settings immediately
  tcsetattr(STDIN_FILENO, TCSANOW, &temp_termios);

  // on exit, restore original settings
  atexit(RestoreTerminalSettings);
}

// TODO prettify this mess and TODO rename parameters
void ProcessDataQuickly(int nfd, int* results,
                        fd_set* reads, const int* my_fds,
                        int* children_pids, int* ready_cnt)
{
  // withoud setting up such a dummy timespec pselect
  // does not return immediately
  struct timespec immediately;
  immediately.tv_sec = 0;
  immediately.tv_nsec = 0;      
  pselect(nfd, reads, NULL, NULL, &immediately, NULL);
          
  for (int i = 0; i < n; i++) {

    // already remembered or not ready to read
    if ((results[i] >= 0) ||
        !(FD_ISSET(my_fds[i], reads)))
      continue;

    struct message_from_child response;
            
    if (read(my_fds[i], &response, sizeof(struct message_from_child)) > 0) {
        
      if (response.value == 0) { // short-circuit
        printf("NULL\n\r");
        for (int j = 0; j < n; ++j) {
          kill(children_pids[j], SIGTERM);
        }
        exit(1);
      }

      results[i] = response.value;
      ++(*ready_cnt);
    }
  }
}