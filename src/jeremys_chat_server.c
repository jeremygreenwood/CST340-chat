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
#include <semaphore.h>
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

typedef enum
{
	srv_quit,
	srv_cont,
	srv_invalid
} client_status_type;


// global variables
worker_t 		g_worker[ MAX_CONN ];	/* pthread structure array	*/
char      		shared_buf[ MAX_LINE ];	/* character buffer         */
sem_t			mutex;


// prototypes
client_status_type process_client_msg( int sock, int id, char *chat_msg );
void *worker_proc( void *arg );
void server_error( char *msg );
void lock_buffer( void );
void unlock_buffer( void );

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
    sem_init( &mutex, 0, 1 );

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


void *worker_proc( void *arg )
{
	int			res;
    char      	msg[ MAX_LINE ];      /*  character buffer          */
	client_status_type
				client_status = srv_cont;
	worker_t *this_thread;

	this_thread = (worker_t *)arg;

	printf( "Client connected on thread %d. \n", this_thread->id );

	int conn_s = this_thread->connection;

	write_client( conn_s, "\nConnected to Jeremy's chat server.  You are logged in as client %d.\n", this_thread->id );

	while( client_status != srv_quit )
	{
		res = Readline( conn_s, msg, MAX_LINE - 1 );

		if( res == CONN_ERR )
			break;

		client_status = process_client_msg( conn_s, this_thread->id, msg );
	}

	fprintf( stderr, "Client on thread %d disconnected. \n", this_thread->id );

	// close the connection
	res = close( conn_s );
	if( res < 0 )
		server_error( "Error calling close()" );

	this_thread->used = FALSE;

	return NULL;
}


client_status_type process_client_msg( int sock, int id, char *chat_msg )
{
	int			i;						/* g_worker index			*/

	// loop through all threads and send message to each that is in use
	for( i = 0; i < MAX_CONN; i++ )
	{
		if( g_worker[ i ].used == TRUE )
		{
			// send chat message to active client (including client who sent message)
			write_client( g_worker[ i ].connection, "Client %d: %s", id, chat_msg );
		}
	}

	return srv_cont;
}


void server_error( char *msg )
{
	sem_destroy( &mutex );
	fprintf( stderr, "%s\n", msg );
	exit( EXIT_FAILURE );
}


void lock_buffer( void )
{
	sem_wait( &mutex );
}


void unlock_buffer( void )
{
	sem_post( &mutex );
}
