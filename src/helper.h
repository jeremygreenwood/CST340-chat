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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>             	/* for ssize_t data type  	*/
#include <sys/socket.h>
#include <errno.h>

#define LISTENQ        	( 1024 )	/* backlog for listen()   	*/

#define MAX_LINE      	( 1024 )	/* maximum string length	*/
#define CONN_ERR		( -1 )		/* connection error			*/


/*  Function declarations  */

ssize_t Readline( int fd, void *vptr, size_t maxlen );
ssize_t Writeline( int fc, const void *vptr, size_t maxlen );
ssize_t write_client( int sock_fd, char *msg, ... );
void set_sock_reuse( int sock_fd );


#endif  /*  PG_SOCK_HELP  */
