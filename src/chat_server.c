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
user_t          user_thread[ MAX_CONN ];/* pthread/user struct array*/
chat_room_t     chatrooms[ MAX_ROOMS ]; /* chatroom struct array    */
chat_room_t     lobby;


int main( int argc, char *argv[] )
{
    int         i;                      /* user_thread index        */
    int         res;                    /* temporary result         */
    int         list_s;                 /* listening socket         */
    int         conn_s;                 /* connection socket        */
    short int   port;                   /* port number              */
    struct      sockaddr_in servaddr;   /* socket address structure */
    char       *endptr;                 /* for strtol()             */

    init_user_thread();

    memset( &chatrooms, 0, sizeof( chatrooms ) );

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

    // create lobby (default) chatroom
    init_chatroom( &lobby, 0, DFLT_CHATROOM_NAME );

    while( 1 )
    {
        // wait for connection
        conn_s = accept( list_s, NULL, NULL );
        if( conn_s < 0 )
            server_error( "Error calling accept()" );

        // search for available thread
        for( i = 0; i < MAX_CONN; i++ )
        {
            if( user_thread[ i ].used == false )
            {
                // found an available thread
                user_thread[ i ].user_id = i;
                user_thread[ i ].connection = conn_s;
                user_thread[ i ].used = true;
                user_thread[ i ].logout = false;
                user_thread[ i ].admin = false;
                user_thread[ i ].login_failure = false;

                // spawn new thread
                pthread_create( &user_thread[ i ].thread, NULL, user_proc, &user_thread[ i ] );
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


void *user_proc( void *arg )
{
    int         result;
    int         conn_s;
    char        msg[ MAX_LINE ];      /*  character buffer          */
    user_t     *this_thread;

    this_thread = (user_t *)arg;

    printf( "Client connected on thread %d, obtaining username... \n", this_thread->user_id );

    conn_s = this_thread->connection;

    get_username( this_thread );

    if( this_thread->login_failure == false )
    {
        // set user's chatroom to lobby (default chatroom)
        if( this_thread->logout == false )
            add_user_to_chatroom( this_thread, &lobby );

        // main loop to receive and process client messages
        while( this_thread->logout == false )
        {
            result = read_client( conn_s, msg );

            if( result == CONN_ERR )
                break;

            process_client_msg( this_thread, msg );
        }

        printf( "%s on thread %d disconnected. \n", this_thread->user_name , this_thread->user_id );
        write_chatroom( this_thread, "%s left the chat. \n", this_thread->user_name );
    }

    // close the connection
    result = close( conn_s );
    if( result < 0 )
        server_error( "Error calling close()" );

    remove_user_from_chatroom( this_thread );
    this_thread->used = false;

    return NULL;
}


void process_client_msg( user_t *user, char *chat_msg )
{
    int     num_args;
    char   *arg_array[ MAX_ARGS ];

    num_args = get_command( chat_msg, arg_array );

    if( num_args > 0 )
    {
        process_command( user, num_args, arg_array );
    }
    else
    {
        write_chatroom( user, "%s: %s", user->user_name, chat_msg );
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


void process_command( user_t *user, int argc, char **argv )
{
#ifdef DEBUG_CMD
    int i;
    printf( "argc: %d \n", argc );
    for( i = 0; i < argc; i++ )
        printf( "argv[ %d ]: <<< %s >>>\n", i, argv[ i ] );
#endif

    // TODO create a struct array of commands which can be automatically enumerated and tested here
    if( strncmp( argv[ 0 ], CMD_LOGOUT, strlen( CMD_LOGOUT ) ) == 0 )
    {
        user->logout = true;
    }
    else if( strncmp(argv[ 0 ], CMD_LIST_ROOMS, strlen( CMD_LIST_ROOMS ) ) == 0 )
    {
        list_chat_rooms( user );
    }
    else if( strncmp( argv[ 0 ], CMD_JOIN_ROOM, strlen( CMD_JOIN_ROOM ) ) == 0 )
    {
        join_chat_room( user, argv[ 1 ] );
    }
    else if( strncmp( argv[ 0 ], CMD_CREATE_ROOM, strlen( CMD_CREATE_ROOM ) ) == 0 )
    {
        create_chat_room( user, argv[ 1 ] );
    }
    else if( strncmp( argv[ 0 ], CMD_LEAVE_ROOM, strlen( CMD_LEAVE_ROOM ) ) == 0 )
    {
        leave_chat_room( user );
    }
    else if( strncmp( argv[ 0 ], CMD_WHERE_AM_I, strlen( CMD_WHERE_AM_I ) ) == 0 )
    {
        where_am_i( user );
    }
    else if( strncmp( argv[ 0 ], CMD_LIST_ROOM_USERS, strlen( CMD_LIST_ROOM_USERS ) ) == 0 )
    {
        list_chat_room_users( user );
    }
    else if( strncmp(argv[ 0 ], CMD_LIST_ALL_USERS, strlen( CMD_LIST_ALL_USERS ) ) == 0 )
    {
        list_all_users( user );
    }
    // XXX next case goes here
//  else if(  )
//  {
//
//  }
    else
    {
        // alert the client the command is invalid
        // TODO show a list of valid commands
        write_client( user->connection, "Invalid command: %s \n", argv[ 0 ] );
    }

}


// write to all clients with locking performed
void write_all_clients( char *msg, ... )
{
    int         i;                      /* user_thread index        */
    char        full_msg[ MAX_LINE ];   /* constructed message      */
    va_list     ap;

    va_start( ap, msg );
    vsprintf( full_msg, msg, ap );
    va_end( ap );

    // loop through all threads and send message to each that is in use
    for( i = 0; i < MAX_CONN; i++ )
    {
        if( user_thread[ i ].used == true )
        {
            sem_wait( &user_thread[ i ].write_mutex );

            // send chat message to active client (including client who sent message)
            write_client( user_thread[ i ].connection, "%s \n", full_msg );

            sem_post( &user_thread[ i ].write_mutex );
        }
    }
}


void server_error( char *msg )
{
    destroy_user_thread();
    fprintf( stderr, "%s\n", msg );
    exit( EXIT_FAILURE );
}


void init_user_thread( void )
{
    int         i;                      /* user_thread index           */

    memset( &user_thread, 0, sizeof( user_thread ) );

    for( i = 0; i < MAX_CONN; i++ )
        sem_init( &user_thread[ i ].write_mutex, 0, 1 );
}


void destroy_user_thread( void )
{
    int         i;                      /* user_thread index           */

    for( i = 0; i < MAX_CONN; i++ )
        sem_destroy( &user_thread[ i ].write_mutex );
}


void get_username( user_t *user )
{
    int         i;
    int         result;
    char        msg[ MAX_LINE ];
    bool        try_again = true;

    while( try_again == true )
    {
        try_again = false;

        // prompt and save client's username
        write_client( user->connection, "\nEnter username: " );

        result = read_client( user->connection, msg );

        if( result == CONN_ERR )
        {
            user->logout = true;
            user->login_failure = true;
            return;
        }

        // verify username is not already in use
        for( i = 0; i < MAX_CONN; i++ )
        {
            if( user_thread[ i ].used && strcmp( user_thread[ i ].user_name, msg ) == 0 )
            {
                write_client( user->connection, "username %s is already in use, please try again. \n", msg );
                try_again = true;
            }
        }
    }

    // set username and respond to client
    strncpy( user->user_name, msg, strlen( msg ) );

    // check username for admin
    admin_check( user );

    if( user->logout == true )
        return;

    write_client( user->connection, "\nConnected to chat server.  You are logged in as %s. \n", user->user_name );

    printf( "%s is running on thread %d.\n", user->user_name, user->user_id );
}


void init_chatroom( chat_room_t *room, int id, char *name )
{
    int i;

    room->room_id = id;
    strncpy( room->room_name, name, MAX_ROOM_NAME_LEN );
    room->user_count = 0;
    for( i = 0; i < MAX_USERS_IN_ROOM; i++ )
        room->users[ i ] = NULL;
    memset( room->history, 0, sizeof( room->history ) );
}


void write_chatroom( user_t *user, char *msg, ... )
{
    int         i;                      /* user_thread index        */
    char        full_msg[ MAX_LINE ];   /* constructed message      */
    va_list     ap;

    va_start( ap, msg );
    vsprintf( full_msg, msg, ap );
    va_end( ap );

    // loop through all users in chatroom
    for( i = 0; i < MAX_USERS_IN_ROOM; i++ )
    {
        // check that each user in the chatroom is used before sending message
        if( user->chat_room->users[ i ] != NULL && user->chat_room->users[ i ]->used == true )
        {
            printf(
                    "writing to %s on thread %d\n",
                    user->chat_room->users[ i ]->user_name,
                    user->chat_room->users[ i ]->user_id
                  );

            sem_wait( &user->chat_room->users[ i ]->write_mutex );

            // send message to user in chatroom (including user who sent message)
            write_client( user->chat_room->users[ i ]->connection, "%s \n", full_msg );

            sem_post( &user->chat_room->users[ i ]->write_mutex );
        }
    }
}


bool list_chat_rooms( user_t *user_submitter )
{
    int     i;
    bool    active_rooms_found = false;

    write_client( user_submitter->connection, "active chatrooms: \n" );

    //search for active chat rooms to print to user_submitter
    for( i = 0; i < MAX_ROOMS; i++ )
    {
        printf( "%d", chatrooms[ i ].user_count );

        if( chatroom_is_active( &chatrooms[ i ] ) )
        {
            write_client( user_submitter->connection, "\t%s \n", chatrooms[ i ].room_name );
            active_rooms_found = true;
            printf( "%s", chatrooms[ i ].room_name );
        }
    }

    if( active_rooms_found == false )
        write_client( user_submitter->connection, "\tno results to display \n" );

    return true;
}


bool create_chat_room( user_t *user_submitter, char *new_name )
{
    if( new_name != NULL )
    {
        int i;
        int room_idx = -1;
        //search for inactive chat room
        for( i = 0; i < MAX_ROOMS; i++ )
        {
            if( chatrooms[ i ].user_count < 1 && room_idx == -1 )
            {
                room_idx = i;
            }
            else if( strncmp( chatrooms[ i ].room_name, new_name, MAX_ROOM_NAME_LEN ) == 0 )
            {
                write_client( user_submitter->connection, "Cannot create room: room with that name already exists! \n" );
                i = MAX_ROOMS + 1;
                room_idx = i;
            }
        }

        if( room_idx < MAX_ROOMS )
        {
            write_client( user_submitter->connection, "Creating chatroom: %s. \n", new_name );
            //initialize the new chat room
            init_chatroom( &chatrooms[ room_idx ], room_idx, new_name );
            // put user in room
            join_chat_room( user_submitter, new_name );
        }
        else if( room_idx == MAX_ROOMS )
        {
            write_client( user_submitter->connection, "Cannot create room: max number of rooms reached! \n" );
        }
    }
    else
    {
        write_client( user_submitter->connection, "Usage: /createchatroom <chatroomname> \n" );
    }
    return true;
}


bool admin_check( user_t *user_submitted )
{
    // declare variables for use in checking password
    char    msg[ sizeof( ADMIN_PASSWORD ) ];
    int     result;

    if( strcmp( user_submitted->user_name, ADMIN_NAME ) == 0 )
    {
        // prompt for password
        write_client( user_submitted->connection, "\nEnter password: " );

        result = read_client( user_submitted->connection, msg );

        if( result == CONN_ERR )
        {
            user_submitted->logout = true;
            user_submitted->login_failure = true;

            return false;
        }

        if( strcmp( msg, ADMIN_PASSWORD ) == 0 )
        {
            user_submitted->admin = true;
            write_client( user_submitted->connection, "\nWelcome Admin! \n" );

            return true;
        }
        else
        {
            write_client( user_submitted->connection, "\nWrong password! \n" );
            user_submitted->logout = true;
            user_submitted->login_failure = true;

            return false;
        }
    }

    return false;
}


bool list_chat_room_users( user_t *user_submitted )
{
    // local variables
    int i;

    // Iterate through user_thread struct and print all users in same room
    for( i = 0; i < MAX_CONN; i++ )
    {
        if( user_submitted->chat_room == user_thread[ i ].chat_room )
        {
            write_client( user_submitted->connection, "%s \n", user_thread[ i ].user_name );
        }
    }

    return true;
}


bool list_all_users( user_t *user_submitter )
{
    // local variables
    int i;

    for( i = 0; i < MAX_CONN; i++ )
    {
        write_client( user_submitter->connection, "%s \n", user_thread[ i ].user_name );
    }

    return true;
}


bool chatroom_is_active( chat_room_t *room )
{
    return room->user_count != 0 ? true : false;
}


bool remove_user_from_chatroom( user_t *user )
{
    int i;

    // check if user is not currently in a chatroom
    if( user->chat_room == NULL )
        return true;

    for( i = 0; i < MAX_USERS_IN_ROOM; i++ )
    {
        if( user->chat_room->users[ i ] == user )
        {
            // announce to the room this user is leaving
            write_chatroom( user, "%s left the chatroom. \n", user->user_name );

            user->chat_room->user_count--;

            user->chat_room->users[ i ] = NULL;
            user->chat_room = NULL;

            return true;
        }
    }

    return false;
}


bool add_user_to_chatroom( user_t *user, chat_room_t *room )
{
    int i;

    for( i = 0; i < MAX_USERS_IN_ROOM; i++ )
    {
        if( room->users[ i ] == NULL )
        {
            // remove user from previous chatroom (if applicable)
            remove_user_from_chatroom( user );

            // set user's chatroom
            user->chat_room = room;

            // add user to chatroom's user* array and increment user_count
            user->chat_room->users[ i ] = user;
            user->chat_room->user_count++;

            printf( "%s joined chatroom %s \n", user->user_name, room->room_name );
            write_client( user->connection, "You have joined chatroom %s. \n", room->room_name );
            write_chatroom( user, "%s has joined the chatroom. \n", user->user_name );

            return true;
        }
    }

    write_client( user->connection, "Error: chatroom %s is full. \n", room );

    return false;
}


bool join_chat_room( user_t *user_submitter, char *room_name )
{
    int i;

    // verify a room name was provided
    if( room_name == NULL )
    {
        write_client( user_submitter->connection, "Usage: %sjoinchatroom <chatroomname> \n", CMD_SIG );

        return false;
    }

    // iterate through all chatrooms to check if chatroom with room_name exists
    for( i = 0; i < MAX_ROOMS; i++ )
    {
        if( strcmp( chatrooms[ i ].room_name, room_name ) == 0 )
        {
            if( add_user_to_chatroom( user_submitter, &chatrooms[ i ] ) )
                return true;
            else
                return false;
        }
    }

    // send message to user_submitter and return false if no rooms were available
    write_client( user_submitter->connection, "Chatroom %s does not exist. \n", room_name );

    return false;
}


bool leave_chat_room( user_t *user_submitter )
{
    remove_user_from_chatroom( user_submitter );

    add_user_to_chatroom( user_submitter, &lobby );

    return true;
}


void where_am_i( user_t *user_submitter )
{
    write_client( user_submitter->connection, "You are in chatroom %s. \n", user_submitter->chat_room->room_name );
}


