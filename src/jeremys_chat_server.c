/*===========================================================================
 Name        : jeremys_server_multi.c
 Author      : Jeremy Greenwood <jeremy.greenwood@oit.edu>
 Course      : CST 340
 Assignment  : 4
 Description : simple multi-threaded server with menu for various functions.
===========================================================================*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */
#include <pthread.h>
#include "helper.h"           /*  our own helper functions  */


// constants
#define MAX_CONN			( 3 )
#define ECHO_PORT      		( 3456 )
#define TRUE				( 1 )
#define FALSE				( 0 )


// types
typedef struct
{
	int			id;
	int			connection;
	pthread_t	thread;
	int 		used;
} worker_t;
worker_t g_worker[ MAX_CONN ];

typedef enum
{
	srv_quit,
	srv_cont,
	srv_invalid
} client_status_type;



char      		shared_buf[ MAX_LINE ];	/* character buffer         */


// prototypes
client_status_type process_client_data( int sock, char *buffer );
void *worker_proc( void *arg );

int main( int argc, char *argv[] )
{
	int			i;						/* g_worker index			*/
	int			res;					/* temporary result			*/
    int			list_s;                	/* listening socket			*/
    int       	conn_s;                	/* connection socket        */
    short int 	port;                  	/* port number              */
    struct    	sockaddr_in servaddr;  	/* socket address structure */
    char	   *endptr;                	/* for strtol()             */

    memset( &g_worker, 0, sizeof( g_worker ) );

    // get port number from command line or set to default port
    if( argc == 2 )
    {
		port = strtol(argv[1], &endptr, 0);
		if( *endptr )
			server_error( "Invalid port number" );
    }
    else if( argc < 2 )
    	port = ECHO_PORT;
    else
    	server_error( "Invalid arguments" );

    // create listening socket
    list_s = socket( AF_INET, SOCK_STREAM, 0 );
    if( list_s < 0 )
    	server_error( "Error creating listening socket" );

    set_sock_reuse( list_s );

    // initialize socket address structure
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);

    //  bind socket address to listening socket
    res = bind( list_s, (struct sockaddr *) &servaddr, sizeof( servaddr ) );
    if( res < 0 )
    	server_error( "Error calling bind()" );

    res = listen( list_s, LISTENQ );
    if( res < 0 )
    	server_error( "Error calling listen()" );

    while( 1 )
    {
		// wait for connection
    	conn_s = accept( list_s, NULL, NULL );
		if( conn_s < 0 )
			server_error( "Error calling accept()" );

		// search for available thread
		for( i = 0; i < MAX_CONN; i++ )
		{
			if( g_worker[ i ].used == FALSE )
			{
				// found an available thread
				g_worker[ i ].id = i;
				g_worker[ i ].connection = conn_s;
				g_worker[ i ].used = TRUE;

				pthread_create( &g_worker[ i ].thread, NULL, worker_proc, &g_worker[ i ] );
				break;
			}
		}

		// turn away excessive connections
		if( i == MAX_CONN )
		{
			printf( "Turned away a client \n" );

			// no threads available, send server busy message to client and close conn_s
			write_client( conn_s, "\nCould not connect to Jeremy's server, all circuits busy. \n" );

			// close the connection
			res = close( conn_s );
			if( res < 0 )
				server_error( "Error calling close()" );
		}
    }
}


client_status_type process_client_data( int sock, char *buffer )
{
	FILE 		*fp;
	char 		ret_buf[ MAX_LINE ];

	// execute menu option received from client
	switch( buffer[0] )
	{
	case '1':
		write_client( sock, "getting server info...\n" );

		fp = popen( "uname -a", "r" );

		while( fgets( ret_buf, sizeof( ret_buf ), fp ) )
			Writeline( sock, ret_buf, strlen( ret_buf ) );

		pclose(fp);

		return srv_cont;

	case '2':
		write_client( sock, "opening cdrom drive...\n" );

		system( "eject" );

		return srv_cont;

	case '3':
		write_client( sock, "listing info for connected drives...\n" );

		fp = popen( "./show_connected_drives.sh", "r" );

		while( fgets( ret_buf, sizeof( ret_buf ), fp ) )
			Writeline( sock, ret_buf, strlen( ret_buf ) );

		pclose( fp );

		return srv_cont;

	case '4':
		write_client( sock, "exiting...\n" );

		return srv_quit;

	default:
		write_client( sock, "'%c' is not a valid option, please try again.\n", buffer[ 0 ] );

		return srv_invalid;
	}
}


void *worker_proc( void *arg )
{
	int			res;
    char      	buffer[ MAX_LINE ];      /*  character buffer          */
	client_status_type
				client_status = srv_cont;
	worker_t *this_thread;

	this_thread = (worker_t *)arg;

	printf( "Client connected on thread %d. \n", this_thread->id );

	int conn_s = this_thread->connection;

	while( client_status != srv_quit )
	{
		write_client(
						conn_s,
						"\nConnected to Jeremy's server.  Execute the following commands: \n" \
						"    1.  get server info \n" \
						"    2.  open the cdrom drive on server \n" \
						"    3.  list drives on server \n" \
						"    4.  exit \n\n:"
					);

		res = Readline( conn_s, buffer, MAX_LINE - 1 );

		if( res == CONN_ERR )
			break;

		client_status = process_client_data( conn_s, buffer );
	}

	fprintf( stderr, "Client on thread %d disconnected. \n", this_thread->id );

	// close the connection
	res = close( conn_s );
	if( res < 0 )
		server_error( "Error calling close()" );

	this_thread->used = FALSE;

	return NULL;
}
