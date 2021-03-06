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
ssize_t read_line( int sockd, void *vptr, size_t maxlen )
{
    ssize_t     n;
    ssize_t     rc;
    char        c;
    char       *buffer;

    buffer = vptr;

    for( n = 1; n < maxlen; n++ )
    {
        rc = read( sockd, &c, 1 );

        if( rc == 1 )
        {
            *buffer++ = c;
            if( c == '\n' )
                break;
        }
        else if( rc == 0 )
        {
            // this case happens when the client closes unexpectedly
            return CONN_ERR;
        }
        else
        {
            if( errno == EINTR )
                continue;
            return CONN_ERR;
        }
    }

    *buffer = 0;
    return n;
}


// write a line to a socket
ssize_t write_line( int sockd, const void *vptr, size_t n )
{
    size_t      nleft;
    ssize_t     nwritten;
    const char *buffer;

    buffer = vptr;
    nleft  = n;

    while( nleft > 0 )
    {
        nwritten = write( sockd, buffer, nleft );

        if( nwritten <= 0 )
        {
            if( errno == EINTR )
                nwritten = 0;
            else
                return -1;
        }
        nleft  -= nwritten;
        buffer += nwritten;
    }

    return n;
}


ssize_t write_client( int sock_fd, char *msg, ... )
{
    char        ret_buf[ MAX_LINE ];
    va_list     ap;

    va_start( ap, msg );
    vsprintf( ret_buf, msg, ap );
    va_end( ap );

    return write_line( sock_fd, ret_buf, strlen( ret_buf ) );
}


ssize_t read_client( int sock_fd, char *msg_dest )
{
    int i;
    int ret_val;

    ret_val = read_line( sock_fd, msg_dest, MAX_LINE - 1 );

    // remove carriage returns and newlines from end of message
    for( i = strlen( msg_dest ) - 1; i >= 0; i-- )
    {
        if( msg_dest[ i ] == '\r' || msg_dest[ i ] == '\n' )
            msg_dest[ i ] = '\0';
        else
            break;
    }

    return ret_val;
}


// Sets up a socket to immediately timeout and become available for reassignment if severed.
// Use this to avoid the address already in use error.
void set_sock_reuse( int sock_fd )
{
    int one = 1;
    setsockopt( sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof( one ) );
}
