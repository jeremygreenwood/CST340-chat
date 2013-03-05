/*===========================================================================
 Filename    : chat_server.c
 Authors     : Jeremy Greenwood <jeremy.greenwood@oit.edu>
             :
 Course      : CST 340
 Assignment  : 6
 Description : Collaborative multi-threaded chat server. Project located at
               https://github.com/jeremygreenwood/CST340-chat
===========================================================================*/

#include "chat_server.h"


// global variables
worker_t        g_worker[ MAX_CONN ];     /* pthread structure array  */


int main( int argc, char *argv[] )
{
    int         i;                      /* g_worker index           */
    int         res;                    /* temporary result         */
    int         list_s;                 /* listening socket         */
    int         conn_s;                 /* connection socket        */
    short int   port;                   /* port number              */
    struct      sockaddr_in servaddr;   /* socket address structure */
    char       *endptr;                 /* for strtol()             */

    init_g_worker();

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
    memset( &servaddr, 0, sizeof( servaddr ) );
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port        = htons( port );

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
            if( g_worker[ i ].used == false )
            {
                // found an available thread
                g_worker[ i ].user.user_id = i;
                g_worker[ i ].connection = conn_s;
                g_worker[ i ].used = true;

                // spawn new thread
                pthread_create( &g_worker[ i ].thread, NULL, worker_proc, &g_worker[ i ] );
                break;
            }
        }

        // turn away excessive connections
        if( i == MAX_CONN )
        {
            printf( "Turned away a client \n" );

            // no threads available, send server busy message to client and close conn_s
            write_client( conn_s, "\nCould not connect to chat server, all circuits busy. \n" );

            // close the connection
            res = close( conn_s );
            if( res < 0 )
                server_error( "Error calling close()" );
        }
    }
}


void *worker_proc( void *arg )
{
    int         result;
    char        msg[ MAX_LINE ];      /*  character buffer          */
    worker_t   *this_thread;

    this_thread = (worker_t *)arg;

    printf( "Client connected on thread %d, obtaining username... \n", this_thread->user.user_id );

    int conn_s = this_thread->connection;
    this_thread->logout = false;

    // TODO set default chatroom

    // prompt and save client's username
    write_client( conn_s, "\nEnter username: " );

    result = read_client( conn_s, msg );

    if( result == CONN_ERR )
    {
        this_thread->logout = true;
    }
    else
    {
        // set username and respond to client
        strncpy( this_thread->user.user_name, msg, strlen( msg ) - 2 );
        write_client( conn_s, "\nConnected to chat server.  You are logged in as %s.\n", this_thread->user.user_name );
        // TODO update write_all_clients() to take a chat_room_t parameter so only clients in a
        // specific chatroom receive the message
        write_all_clients( "%s joined the chat.\n", this_thread->user.user_name );

        printf( "%s is running on thread %d.\n", this_thread->user.user_name, this_thread->user.user_id );
    }

    while( this_thread->logout == false )
    {
        result = read_client( conn_s, msg );

        if( result == CONN_ERR )
            break;

        process_client_msg( this_thread, msg );
    }

    printf( "%s on thread %d disconnected. \n", this_thread->user.user_name , this_thread->user.user_id );

    write_all_clients( "%s left the chat. \n", this_thread->user.user_name );

    // close the connection
    result = close( conn_s );
    if( result < 0 )
        server_error( "Error calling close()" );

    this_thread->used = false;

    return NULL;
}


void process_client_msg( worker_t *worker, char *chat_msg )
{
    int     num_args;
    char   *arg_array[ MAX_ARGS ];

    num_args = get_command( chat_msg, arg_array );

    if( num_args > 0 )
    {
        process_command( worker, num_args, arg_array );
    }
    else
    {
        // TODO write to current chatroom only
        write_all_clients( "%s: %s", worker->user.user_name, chat_msg );
    }
}


// tokenize the message if the command signature is found
int get_command( char *msg, char **argv )
{
    int num_args = 0;

    if( strncmp( msg, CMD_SIG, strlen( CMD_SIG ) ) )
        return 0;

    argv[ num_args ] = strtok( msg + strlen( CMD_SIG ), " " );

    while( argv[ num_args++ ] )
        argv[ num_args ] = strtok( NULL, " " );

    return --num_args;
}


void process_command( worker_t *worker, int argc, char **argv )
{
#ifdef DEBUG_CMD
    int i;
    printf( "argc: %d \n", argc );
    for( i = 0; i < argc; i++ )
        printf( "argv[ %d ]: %s \n", i, argv[ i ] );
#endif

    if( strncmp( argv[ 0 ], CMD_LOGOUT, strlen( CMD_LOGOUT ) ) == 0 )
    {
        worker->logout = true;
    }
    // XXX next case goes here
//  else if(  )
//  {
//
//  }
    // XXX next case goes here
//  else if(  )
//  {
//
//  }
    else
    {
        // alert the client the command is invalid
        // TODO show a list of valid commands
        write_client( worker->connection, "Invalid command: %s \n", argv[ 0 ] );
    }

}


// write to all clients with locking performed
void write_all_clients( char *msg, ... )
{
    int         i;                      /* g_worker index           */
    char        full_msg[ MAX_LINE ];   /* constructed message      */
    va_list     ap;

    va_start( ap, msg );
    vsprintf( full_msg, msg, ap );
    va_end( ap );

    // loop through all threads and send message to each that is in use
    for( i = 0; i < MAX_CONN; i++ )
    {
        if( g_worker[ i ].used == true )
        {
            sem_wait( &g_worker[ i ].write_mutex );

            // send chat message to active client (including client who sent message)
            write_client( g_worker[ i ].connection, full_msg );

            sem_post( &g_worker[ i ].write_mutex );
        }
    }
}


void server_error( char *msg )
{
    destroy_g_worker();
    fprintf( stderr, "%s\n", msg );
    exit( EXIT_FAILURE );
}


void init_g_worker( void )
{
    int         i;                      /* g_worker index           */

    memset( &g_worker, 0, sizeof( g_worker ) );

    for( i = 0; i < MAX_CONN; i++ )
        sem_init( &g_worker[ i ].write_mutex, 0, 1 );
}


void destroy_g_worker( void )
{
    int         i;                      /* g_worker index           */

    for( i = 0; i < MAX_CONN; i++ )
        sem_destroy( &g_worker[ i ].write_mutex );
}
