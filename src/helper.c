/*

  HELPER.C
  ========
  (c) Paul Griffiths, 1999
  Email: mail@paulgriffiths.net

  Implementation of sockets helper functions.

  Many of these functions are adapted from, inspired by, or 
  otherwise shamelessly plagiarised from "Unix Network 
  Programming", W Richard Stevens (Prentice Hall).

*/

#include "helper.h"


// read a line from a socket
ssize_t Readline(int sockd, void *vptr, size_t maxlen)
{
//	int			res;
    ssize_t 	n;
    ssize_t 	rc;
    char    	c;
    char       *buffer;

    buffer = vptr;

    for( n = 1; n < maxlen; n++ )
    {
    	rc = read( sockd, &c, 1 );

		// TODO remove this
		fprintf( stderr, "Error: rc:       %ld \n", rc );
		fprintf( stderr, "     : c(ascii): %c  \n", c  );
		fprintf( stderr, "     : c(hex):   %x  \n", c  );

		if( rc == 1 )
		{
			*buffer++ = c;
			if( c == '\n' )
				break;
		}
		else if( rc == 0 )
		{
			// This case happens when the client unexpectedly closes.
			// Currently this crashes the server.
			// In this case the thread should be freed.
			if ( n == 1 )
				return 0;
			else
				break;
		}
		else
		{
			if( errno == EINTR )
				continue;
			return -1;
		}
    }

    *buffer = 0;
    return n;
}


// write a line to a socket
ssize_t Writeline(int sockd, const void *vptr, size_t n)
{
    size_t    	nleft;
    ssize_t     nwritten;
    const char *buffer;

    buffer = vptr;
    nleft  = n;

    while( nleft > 0 )
    {
		if( (nwritten = write(sockd, buffer, nleft)) <= 0 )
		{
			if ( errno == EINTR )
				nwritten = 0;
			else
				return -1;
		}
		nleft  -= nwritten;
		buffer += nwritten;
    }

    return n;
}


void server_error(char *msg)
{
	fprintf( stderr, "%s\n", msg );
	exit( EXIT_FAILURE );
}
