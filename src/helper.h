/*

  HELPER.H
  ========
  (c) Paul Griffiths, 1999
  Email: paulgriffiths@cwcom.net

  Interface to socket helper functions.

  Many of these functions are adapted from, inspired by, or
  otherwise shamelessly plagiarised from "Unix Network
  Programming", W Richard Stevens (Prentice Hall).

*/


#ifndef PG_SOCK_HELP
#define PG_SOCK_HELP


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>             /*  for ssize_t data type  */
#include <sys/socket.h>
#include <errno.h>

#define LISTENQ        (1024)   /*  Backlog for listen()   */


/*  Function declarations  */

ssize_t Readline(int fd, void *vptr, size_t maxlen);
ssize_t Writeline(int fc, const void *vptr, size_t maxlen);
void server_error(char *msg);


#endif  /*  PG_SOCK_HELP  */
