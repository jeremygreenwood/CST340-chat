/*===========================================================================
 Filename    : chat_server.c
 Authors     : Jeremy Greenwood <jeremy.greenwood@oit.edu>
             : Joshua Durkee    <joshua.durkee@oit.edu>
 Course      : CST 340
 Assignment  : 6
 Description : Collaborative multi-threaded chat server. Project located at
 https://github.com/jeremygreenwood/CST340-chat
 ===========================================================================*/

#include "chat_server.h"

// global variables
user_t user_thread[ MAX_CONN ];/* pthread/user struct array*/
chat_room_t chatrooms[ MAX_ROOMS ]; /* chatroom struct array    */
chat_room_t lobby;
blocked_ip_t blocks[ MAX_BLOCKED ];


int main( int argc, char *argv[ ] )
{
    int                 i;          /* user_thread index        */
    int                 res;        /* temporary result         */
    int                 list_s;     /* listening socket         */
    int                 conn_s;     /* connection socket        */
    short int           port;       /* port number              */
    struct sockaddr_in  servaddr;   /* socket address structure */
    char               *endptr;     /* for strtol()             */
    struct sockaddr_in  client_addr;
    socklen_t           c_len = sizeof( client_addr );

    init_user_thread();

    memset( &chatrooms, 0, sizeof( chatrooms ) );
    memset( &blocks, 0, sizeof( blocks ) );

    // get port number from command line or set to default port
    if( argc == 2 )
    {
        port = strtol( argv[ 1 ], &endptr, 0 );
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
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( port );

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
        conn_s = accept( list_s, (struct sockaddr*)&client_addr, &c_len );
        if( conn_s < 0 )
            server_error( "Error calling accept()" );

        printf( "client_addr: %d \n", client_addr.sin_addr.s_addr );
        printf( "client_addr: %s \n", inet_ntoa( client_addr.sin_addr ) );

        // search for available thread
        for( i = 0; i < MAX_CONN; i++ )
        {
            if( user_thread[ i ].used == false )
            {
                // found an available thread
                user_thread[ i ].user_ip_addr = client_addr.sin_addr;
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
    int result;
    int conn_s;
    char msg[ MAX_LINE ]; /*  character buffer          */
    user_t *this_thread;

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

            // deep copy msg to this_thread
            memset( this_thread->user_msg, 0, BUFFER_SIZE);
            strcpy( this_thread->user_msg, msg );

            process_client_msg( this_thread, msg );
        }
    }

    printf( "%s on thread %d disconnected, resetting all values. \n", this_thread->user_name, this_thread->user_id );

    write_chatroom( this_thread, "%s left the chat.", this_thread->user_name );
    reset_user( this_thread );

    // close the connection
    result = close( conn_s );
    if( result < 0 )
        server_error( "Error calling close()" );

    return NULL ;
}

void process_client_msg( user_t *user, char *chat_msg )
{
    int num_args;
    char *arg_array[ MAX_ARGS ];

    // throw away empty chat messages, assuming the user inadvertently pressed enter
    if( strcmp( chat_msg, "" ) != 0 )
    {
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
}

// tokenize the message if the command signature is found
int get_command( char *msg, char **argv )
{
    int num_args = 0;

    if( strncmp( msg, CMD_SIG, strlen( CMD_SIG ) ) )
        return 0;

    argv[ num_args ] = strtok( msg + strlen( CMD_SIG ), " " );

    while( (num_args < MAX_ARGS)&&(argv[ num_args++ ] ))
        argv[ num_args ] = strtok( NULL, " " );

    return --num_args;
}

void process_command( user_t *user, int argc, char **argv )
{
#ifdef DEBUG_CMD
    int j;
    printf( "argc: %d \n", argc );
    for( j = 0; j < argc; j++ )
    printf( "argv[ %d ]: <<< %s >>>\n", j, argv[ j ] );
#endif

    int i;
    int ret_val;
    int num_commands = sizeof( commands ) / sizeof(command_t);

    for( i = 0; i < num_commands; i++ )
    {
        if( strcicmp( argv[ 0 ], commands[ i ].command_string ) == 0 )
        {
            // execute desired command
            ret_val = commands[ i ].command_function( user, argc, argv );

            if( ret_val == DISPLAY_USAGE )
                write_client( user->connection, "Usage: %s%s %s \n", CMD_SIG, argv[ 0 ], commands[ i ].command_parameter_usage );

            break;
        }
    }
    
    // catch unknown commands
    if( i == num_commands )
    {
        write_client( user->connection, "Invalid command: %s \n", argv[ 0 ] );
        write_client( user->connection, "type \"help\" for a list of commands. \n" );
    }
}

// write to all clients with locking performed
void write_all_clients( char *msg, ... )
{
    int i; /* user_thread index        */
    char full_msg[ MAX_LINE ]; /* constructed message      */
    va_list ap;

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
    int i; /* user_thread index           */

    memset( &user_thread, 0, sizeof( user_thread ) );

    for( i = 0; i < MAX_CONN; i++ )
        sem_init( &user_thread[ i ].write_mutex, 0, 1 );
}

void destroy_user_thread( void )
{
    int i; /* user_thread index           */

    for( i = 0; i < MAX_CONN; i++ )
        sem_destroy( &user_thread[ i ].write_mutex );
}

void get_username( user_t *user )
{
    int     i;
    int     result;
    char    msg[ MAX_LINE ];
    bool    try_again = true;

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

        // verify username is not greater than the maximum number of allowed characters
        if( strlen( msg ) >= MAX_USER_NAME_LEN )
        {
            write_client( user->connection, "Error: exceeded maximum username length of %d characters, please try again. \n", MAX_USER_NAME_LEN );
            try_again = true;
            continue;
        }

        // verify username is not already in use
        for( i = 0; i < MAX_CONN; i++ )
        {
            if( ( true == user_thread[ i ].used ) && ( strcicmp( user_thread[ i ].user_name, msg ) == 0 ) )
            {
                write_client( user->connection, "username %s is already in use, please try again. \n", msg );
                try_again = true;
                continue;
            }
        }

        // verify username is alphanumeric
        for( i = 0; i < strlen( msg ); i++ )
        {
            if( !isalnum( msg[ i ] ) )
            {
                write_client( user->connection, "Invalid character: %c, user name must be alphanumeric. \n", msg[ i ] );
                try_again = true;
                continue;
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

bool admin_check( user_t *user_submitted )
{
    // declare variables for use in checking password
    char msg[ sizeof( ADMIN_PASSWORD ) ];
    int result;

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

void init_chatroom( chat_room_t *room, int id, char *name )
{
    int i;

    room->room_id = id;
    strncpy( room->room_name, name, MAX_ROOM_NAME_LEN );
    room->user_count = 0;
    for( i = 0; i < MAX_USERS_IN_ROOM; i++ )
        room->users[ i ] = NULL;
    sem_init( &room->history_mutex, 0, 1 );
}

void write_chatroom( user_t *user, char *msg, ... )
{
    int i; /* user_thread index        */
    char full_msg[ MAX_LINE ]; /* constructed message      */
    va_list ap;

    va_start( ap, msg );
    vsprintf( full_msg, msg, ap );
    va_end( ap );

    // loop through all users in chatroom
    for( i = 0; i < MAX_USERS_IN_ROOM; i++ )
    {
        // check that each user in the chatroom is used before sending message
        if( user->chat_room->users[ i ] != NULL && user->chat_room->users[ i ]->used == true )
        {
            // Filter out unwanted messages from ignore list            
            if ( (!is_ignoring_user_name(user->chat_room->users[i], user->user_name ))&&(!is_ignoring_user_name(user, user->chat_room->users[i]->user_name)))            
            {                
                printf( "writing to %s on thread %d\n", user->chat_room->users[ i ]->user_name, user->chat_room->users[ i ]->user_id );
                sem_wait( &user->chat_room->users[ i ]->write_mutex );
                // send message to user in chatroom (including user who sent message)                
                write_client( user->chat_room->users[ i ]->connection, "%s \n", full_msg );
                sem_post( &user->chat_room->users[ i ]->write_mutex );            
            }        
        }

    }

    // Write the message to the next available line of chatroom's history
    sem_wait( &user->chat_room->history_mutex );
        int j; /* for loop counter */
        for (j = 0; j < MAX_LINE; j++ )
        {
            user->chat_room->history[user->chat_room->history_count][j] = '\0';
        }
		time_t ltime;           /* calendar time */
		ltime = time(NULL);
		char timestamp[15];     /* buffer for timestamp string */
		strftime(timestamp, 15, "%a %I:%M %p", localtime(&ltime)); /* populate timestamp string */
		sprintf(user->chat_room->history[user->chat_room->history_count], "[%s] %s", timestamp, full_msg);

        user->chat_room->history_count = (user->chat_room->history_count + 1) % HISTORY_SIZE;

    sem_post( &user->chat_room->history_mutex );
}

bool chatroom_is_active( chat_room_t *room )
{
    return room->user_count != 0 ? true : false;
}

int remove_user_from_chatroom( user_t *user )
{
    int i;
    struct chat_room_t *room_pointer;

    // check if user is not currently in a chatroom
    if( user->chat_room == NULL )
        return FAILURE;

    room_pointer = user->chat_room;

    for( i = 0; i < MAX_USERS_IN_ROOM; i++ )
    {
        if( user->chat_room->users[ i ] == user )
        {
            // announce to the room this user is leaving
            write_chatroom( user, "%s left the chatroom.", user->user_name );
            user->chat_room->user_count--;

            user->chat_room->users[ i ] = NULL;
            user->chat_room = NULL;

            return SUCCESS;
        }
    }

    if( room_pointer->user_count == 0 )
    {
        sem_destroy( &room_pointer->history_mutex );
    }

    return SUCCESS;
}

int add_user_to_chatroom( user_t *user, chat_room_t *room )
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
            write_chatroom( user, "%s has joined the chatroom.", user->user_name );

            return SUCCESS;
        }
    }

    write_client( user->connection, "Error: chatroom %s is full. \n", room );

    return FAILURE;
}

// ********** COMMANDS *************

int help( user_t *user_submitter, int argc, char **argv )
{
    int i;
    int num_commands = sizeof( commands ) / sizeof(command_t);

    switch( argc )
    {
    case 1:
        write_client( user_submitter->connection, "available commands: \n" );

        for( i = 0; i < num_commands; i++ )
        {
            write_client( user_submitter->connection, "\t%s \n", commands[ i ].command_string );
        }

        return SUCCESS;

    case 2:
        for( i = 0; i < num_commands; i++ )
        {
            if( strcicmp( commands[ i ].command_string, argv[ 1 ] ) == 0 )            
            {
                write_client( user_submitter->connection, "Usage: %s%s %s \n", CMD_SIG, commands[ i ].command_string , commands[ i ].command_parameter_usage );                
                break;
            }
        }

        // catch unknown commands
        if( i == num_commands )
        {
            write_client( user_submitter->connection, "Invalid command: %s \n", argv[ 1 ] );

            return FAILURE;
        }
        else
            return SUCCESS;
    }

    return DISPLAY_USAGE;
}

int logout( user_t *user_submitter, int argc, char **argv )
{
    user_submitter->logout = true;
    user_submitter->used = false;
    return SUCCESS;
}

int kick_user( user_t *user_submitter, int argc, char **argv )
{
    int i;
    int result = FAILURE;
    char *user_name = argv[ 1 ];

    // verify a user name was provided and the number of args is correct
    if( user_name == NULL || argc != 2)
    {
        return DISPLAY_USAGE;
    }

    // iterate through all users to find the one targeted for kick
    for( i = 0; i < MAX_CONN; i++ )
    {
        if( strcmp( user_thread[ i ].user_name, user_name ) == 0 )
        {
            // Call logout command on the targeted user
            result = logout( &user_thread[ i ], argc, argv );

            if( result == SUCCESS )
            {
                write_client( user_submitter->connection, "User %s was kicked. \n", user_name );
                return result;
            }
        }
    }

    // send message to user_submitter and return targeted user was not found
    write_client( user_submitter->connection, "Could not find %s. \n", user_name );

    return result;
}

int kick_all_users_in_chat_room( user_t *user_submitter, int argc, char **argv )
{
    int i;
    int result = FAILURE;
    char *room_name = argv[ 1 ];
    chat_room_t *room = NULL;

    // verify a room name was provided
    if( room_name == NULL || argc != 2)
    {
        return DISPLAY_USAGE;
    }

    // iterate through all chatrooms to check if chatroom with room_name exists
    for( i = 0; i < MAX_ROOMS; i++ )
    {
        if( strcmp( chatrooms[ i ].room_name, room_name ) == 0 )
        {
            room = &chatrooms[ i ];
            break;
        }
    }

    // Could not find the room
    if( room == NULL )
    {
        write_client( user_submitter->connection, "Chatroom %s does not exist. \n", room_name );
        return FAILURE;
    }
    struct user_t *current_user;
    // Run the logout command on each user one by one
    for( i = 0; i < room->user_count; i++ )
    {
        if( room->users[ i ] != NULL )
        {
            current_user = room->users[i];
            result = logout( current_user, argc, argv );
            if( result == SUCCESS )
            {
                write_client( user_submitter->connection, "User %s was kicked. \n", room->users[ i ]->user_name );
            }
        }
    }

    return SUCCESS;
}

int list_chat_rooms( user_t *user_submitter, int argc, char **argv )
{
    int i;
    bool active_rooms_found = false;

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

    return SUCCESS;
}

int create_chat_room( user_t *user_submitter, int argc, char **argv )
{
    int     i;
    int     room_idx = -1;
    char   *new_name = argv[ 1 ];

    if( new_name != NULL )
    {
        // verify chatroom name is not greater than the maximum number of allowed characters
        if( strlen( new_name ) >= MAX_ROOM_NAME_LEN )
        {
            write_client(
                            user_submitter->connection,
                            "Error: exceeded maximum chatroom name length of %d characters. \n",
                            MAX_ROOM_NAME_LEN
                        );

            return FAILURE;
        }

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

                return FAILURE;
            }
        }

        if( room_idx == -1 )
        {
            write_client( user_submitter->connection, "Cannot create room: max number of rooms reached! \n" );

            return FAILURE;
        }
        else if( room_idx < MAX_ROOMS )
        {
            write_client( user_submitter->connection, "Creating chatroom: %s. \n", new_name );

            //initialize the new chat room
            init_chatroom( &chatrooms[ room_idx ], room_idx, new_name );

            // put user in room
            join_chat_room( user_submitter, argc, argv );
        }
    }
    else
    {
        return DISPLAY_USAGE;
    }

    return SUCCESS;
}

int join_chat_room( user_t *user_submitter, int argc, char **argv )
{
    int i;
    char *room_name = argv[ 1 ];

    // verify a room name was provided
    if( room_name == NULL )
    {
        return DISPLAY_USAGE;
    }

    // iterate through all chatrooms to check if chatroom with room_name exists
    for( i = 0; i < MAX_ROOMS; i++ )
    {
        if( strcmp( chatrooms[ i ].room_name, room_name ) == 0 )
        {
            if( add_user_to_chatroom( user_submitter, &chatrooms[ i ] ) )
                return SUCCESS;
            else
                return FAILURE;
        }
    }

    // send message to user_submitter and return failure if no rooms were available
    write_client( user_submitter->connection, "Chatroom %s does not exist. \n", room_name );

    return FAILURE;
}

int leave_chat_room( user_t *user_submitter, int argc, char **argv )
{
    int ret_val;

    ret_val = remove_user_from_chatroom( user_submitter );

    if( ret_val == FAILURE )
        return ret_val;

    ret_val = add_user_to_chatroom( user_submitter, &lobby );

    return ret_val;
}

int where_am_i( user_t *user_submitter, int argc, char **argv )
{
    write_client( user_submitter->connection, "You are in chatroom %s. \n", user_submitter->chat_room->room_name );

    return SUCCESS;
}

int list_chat_room_users( user_t *user_submitter, int argc, char **argv )
{
    int i;

    // Iterate through user_thread struct and print all users in same room
    for( i = 0; i < MAX_CONN; i++ )
    {
        if( user_submitter->chat_room == user_thread[ i ].chat_room && user_thread[ i ].used == true )
        {
            write_client( user_submitter->connection, "\t%s \n", user_thread[ i ].user_name );        
        }
    }

    return SUCCESS;
}

int list_all_users( user_t *user_submitter, int argc, char **argv )
{
    bool first_line = true;    
    int i;

    for( i = 0; i < MAX_CONN; i++ )
    {
        if ( true == user_thread[i].used)        
        {            
            if ( true == first_line )            
            {                
                first_line = false;                
                write_client( user_submitter->connection, "--- All Online Users --- \n" );            
            }            
            if ( is_ignoring_user_name( user_submitter, user_thread[i].user_name ) )            
            {                
                write_client( user_submitter->connection, "%s\t (ignored) \n", user_thread[ i ].user_name );            
            }            
            else            
            {                
                if ( is_ignoring_user_name(&user_thread[i], user_submitter->user_name) )                    
                    write_client( user_submitter->connection, "%s\t (ignoring you) \n", user_thread[ i ].user_name );                
                else                    
                    write_client( user_submitter->connection, "%s \n", user_thread[ i ].user_name );            
            }        
        }    
    }

    return SUCCESS;
}

int whisper_user( user_t *user_submitter, int argc, char **argv )
{
    int     i;
    char   *target_user_name = argv[ 1 ];
    char   *message;

    if( argc < 3 )
    {
        return DISPLAY_USAGE;
    }

    // Fail if target user is not logged in    
    user_t *whisper_target = NULL;
    if( !is_logged_in( argv[ 1 ], &whisper_target ) )
    {
        write_client( user_submitter->connection, "Cannot send message. %s is not logged in. \n", argv[ 1 ] );
        return FAILURE;
    }

    // Fail if target user is being ignored    
    if( ( NULL != whisper_target ) && ( is_ignoring_user_name( user_submitter, whisper_target->user_name ) ) )
    {
        write_client( user_submitter->connection, "Cannot send message. You are ignoring %s. \n", argv[ 1 ] );
        return FAILURE;
    }

    // Don't talk to yourself
    if( 0 == strcicmp( user_submitter->user_name, whisper_target->user_name ) )
    {
        write_client( user_submitter->connection, "Talking to yourself? \n" );
        return FAILURE;
    }

    // Grab the full message string from user and chop off first 2 parameters
    int offset = strlen( argv[ 0 ] ) + 1 + strlen( argv[ 1 ] );
    message = strstr( user_submitter->user_msg + offset, argv[ 2 ] );

    for( i = 0; i < MAX_CONN; i++ )
    {
        if( strcicmp( target_user_name, user_thread[ i ].user_name ) == 0 )
        {
            // Suceed but don't actually send message if target is ignoring user            
            if( !is_ignoring_user_name( &user_thread[ i ], user_submitter->user_name ) )
            {
                write_client( user_thread[ i ].connection, "(%s: %s) \n", user_submitter->user_name, message );
                user_thread[ i ].reply_user = user_submitter;
            }
            return SUCCESS;
        }
    }

    write_client( user_submitter->connection, "Cannot send message: no user with the specified user name found. \n" );
    return FAILURE;
}

int reply_user( user_t *user_submitter, int argc, char **argv )
{
    char   *message;

    if( argc < 2 )
    {
        return DISPLAY_USAGE;
    }

    // Grab the full message string from user and chop off the first parameter
    int offset = strlen( argv[ 0 ] );
    message = strstr( user_submitter->user_msg + offset, argv[ 1 ] );

    //Send message
    if( user_submitter->reply_user != NULL )
    {
        write_client( user_submitter->reply_user->connection, "(%s: %s) \n", user_submitter->user_name, message );
        user_submitter->reply_user->reply_user = user_submitter;
        return SUCCESS;
    }

    write_client( user_submitter->connection, "Cannot send message: no user has whispered you. \n" );
    return FAILURE;
}

/***********************************************************************
* mute_user - Add a given user to the current user's mute list
*
* parameters:
*   user_submitter - pointer to a user_t containing the current user
*   argc           - int containing # of arguments to the command
*   argv           - vector containing remaining string arguments
*
* returns: An int indicating varius error states:
*    DISPLAY_USAGE : wrong number of parameters given (must be 1 or 2)
*    FAILURE: can't add the specified user to the mute list
*    SUCCESS: target user was added to user_submitter's mute list
*
* The function will return FAILURE if the target user is not logged in,
* or the user has tried to mute herself, or the user attempts to mute
* someone that is already muted, or there is no room left in the mute
* list for the user_submitter
*
***********************************************************************/
int mute_user( user_t *user_submitter, int argc, char **argv )
{
    int i;                              /* loop counter */
    user_t *mute_user_pointer = NULL;   /* pointer to user we will mute */

    // Prompt if they gave wrong arguments
    if( 2 < argc )
        return DISPLAY_USAGE;

    // Using the command with no arguments blats out the list of muted users
    if( 1 == argc )
    {
        print_mute_list( user_submitter );
        return SUCCESS;
    }

    // Fail if the user is not logged in
    if( !is_logged_in( argv[ 1 ], &mute_user_pointer ) )
    {
        write_client( user_submitter->connection, "ERROR: Cannot mute %s. User is not logged in. \n", argv[ 1 ] );
        return FAILURE;
    }

    // Fail if they're trying to mute themselves. Silly.
    if( mute_user_pointer == user_submitter )
    {
        write_client( user_submitter->connection, "You can't mute yourself. \n" );
        return FAILURE;
    }
    
    // Fail if they're trying to mute the administrator
    if ( 0 == strcicmp(mute_user_pointer->user_name, ADMIN_NAME) )
    {
        write_client( user_submitter->connection, "Cannot mute %s. Nobody puts %s in a corner. \n", ADMIN_NAME, ADMIN_NAME);
        return FAILURE;
    }

    // Fail if the submitting user is already ignoring the target user
    if( is_ignoring_user_name( user_submitter, mute_user_pointer->user_name ) )
    {
        write_client( user_submitter->connection, "ERROR: You are already ignoring %s. \n", argv[ 1 ] );
        return FAILURE;
    }

    // Find a blank place in the user's mute list and stick 'em in
    i = 0;
    bool found = false; /* for output of mute list */
    while( ( i < MAX_CONN ) && ( !found ) )
    {
        if( '\0' == user_submitter->muted_users[ i ][ 0 ] )
            found = true;
        i++;

    }

    if( false == found )  // Mute list is full
    {
        write_client( user_submitter->connection, "ERROR: Can't mute %s. Your mute list is full. \n", argv[ 1 ] );
        return FAILURE;
    }

    // If we got this far, we can go ahead and mute the user and let everybody know.
    strcpy( user_submitter->muted_users[ --i ], mute_user_pointer->user_name );
    write_client( mute_user_pointer->connection, "%s is ignoring you. \n", user_submitter->user_name );
    write_client( user_submitter->connection, "You are now ignoring %s. \n", argv[ 1 ] );
    return SUCCESS;
}

int unmute_user( user_t *user_submitter, int argc, char **argv )
{    
    // Prompt if they gave wrong arguments    
    if ( 2 < argc )        
        return DISPLAY_USAGE;    
        
    // Using the command with no arguments blats out the list of muted users    
    if ( 1 == argc )    
    {        
        print_mute_list( user_submitter );        
        return SUCCESS;    
    }    
    
    // Fail if the given username isn't in the user's mute list
    if ( !is_ignoring_user_name( user_submitter, argv[1] ))
    {
        write_client( user_submitter->connection, "Error. %s is not muted. \n", argv[1]);
        return FAILURE;
    }
    
    
    int i=0;                      /* loop counter */
    user_t *other_user = NULL;
    while ( i < MAX_CONN)
    {
        if ( 0 == strcicmp(user_submitter->muted_users[i], argv[1]) )
        {
            printf( "after strcicmp \n");
            user_submitter->muted_users[i][0] = '\0';
            
            if ( is_logged_in(argv[1], &other_user))
            {
                printf( "user is logged in \n");
                if (NULL != other_user)
                    write_client(other_user->connection, "%s has stopped ignoring you. \n", user_submitter->user_name);
            }
            write_client( user_submitter->connection, "You are no longer ignoring %s. \n", argv[1]);
            return SUCCESS;
        }
        i++;
    }
    
    write_client( user_submitter->connection, "Error. Cannot unmute %s. \n", argv[1]);
    return FAILURE;
}

/***********************************************************************
* print_mute_list - Prints current user's list of muted users
*
* parameters:
*   user_submitter - pointer to a user_t struct that will holds a ref
*                    to the user whose mute list is being printed
*
* returns: none
*
* The function writes out the user_submitter's mute list to their
* connection.
*
***********************************************************************/
void print_mute_list( user_t *user_submitter )
{    
    // If we didn't get a valid pointer, don't try to use it    
    if (NULL == user_submitter)        
        return;    
    
    int i;                          /* loop counter */    
    bool first_line = true;    
    for ( i = 0; i < MAX_CONN; i++ )    
    {        
        if ( '\0' != user_submitter->muted_users[i][0] )        
        {            
            if ( true == first_line )            
            {                
                write_client( user_submitter->connection, "--- Muted Users ---- \n");                
                first_line = false;            
            }            
            write_client( user_submitter->connection, "\t %s \n", user_submitter->muted_users[i] );        
        }    
    }    
    
    if ( true == first_line )        
        write_client( user_submitter->connection, "You haven't muted anyone yet. \n");    
    return;
}

/***********************************************************************
* is_logged_in - indicate whether a user with the given name is connected
*
* parameters:
*   user_name - character string with target user's name
*   user_pointer - pointer to a user_t struct that will hold a ref
*                  to the user if it is found. CHANGED BY FUNCTION
*
* returns: A bool indicating whether the user_pointer is pointing at
*          a valid logged in user with the given name
*
* The function checks to see if there are any logged-in users with the
* given name (regardless of case - aMY and Amy are the same to this fn)
* If a match is found, the user_pointer will point at the matching user
* If no match is found, the user_pointer will be set to NULL
*
***********************************************************************/
bool is_logged_in( char *user_name, user_t **user_pointer )
{
    *user_pointer = NULL;
    int i = 0;
    bool match_found = false;

    // Loop over all users; find one that is connected and has same name
    while( ( !match_found ) & ( i < MAX_CONN ) )
    {
        if( ( true == user_thread[ i ].used ) && ( strcicmp( user_name, user_thread[ i ].user_name ) == 0 ) )
        {
            *user_pointer = &user_thread[ i ];
            match_found = true;
        }
        i++;
    }

    // Indicate whether we found a match
    return match_found;
}

/***********************************************************************
* is_ignoring_user_name - indicate whether given name is in ignore list*
* parameters:
*   user_ignoring - pointer to a user_t that is doing the ignoring
*   user_ignored  - pointer to a user_t for the person being ignored
*   ignore_name   - char * that has name of person being ignored*
* returns: A bool indicating whether  the first user is ignoring the
*          second user
*          second user. Does case-insensitive comparison, so Amy == amy*
***********************************************************************/
bool is_ignoring_user_name( user_t *user_ignoring, char *ignore_name )
{
    // Can't ignore a blank user    
    if( ( NULL == user_ignoring ) || ( '\0' == ignore_name ) )
        return false;

    bool user_found_in_mute_list = false;
    int i = 0;

    while( ( !user_found_in_mute_list ) && ( i < MAX_CONN ) )
    {
        if( 0 == strcicmp( user_ignoring->muted_users[ i ], ignore_name ) )
            user_found_in_mute_list = true;

        i++;
    }

    return user_found_in_mute_list;
}

int get_history( user_t *user_submitter, int argc, char **argv )
{
    int i = 0; /* loop counter */
    int line_num;
    int total_lines;
    struct chat_room_t *user_room = user_submitter->chat_room;

    // Make sure the user has a valid room first
    if ( NULL == user_room )
        return FAILURE;

    // Fail if they gave too many arguments    
    if ( argc > 2 )        
        return DISPLAY_USAGE;
    
    char header[80];    /* For printing title */
    // If they didn't provide a # of lines, print all available history
    if ( argc == 1 ) 
    {
        line_num = user_room->history_count;
        total_lines = HISTORY_SIZE;
    }
    else    // Otherwise, print the # of requested lines
    {        
		total_lines = atoi(argv[1]);
		if (total_lines > HISTORY_SIZE)
		    total_lines = HISTORY_SIZE;
            
        line_num = (user_room->history_count - 1 + HISTORY_SIZE) % HISTORY_SIZE;
    }
	// Go backwards, count up how many non-blank lines can be printed before this line
    while ((line_num != user_room->history_count)&&( i < total_lines ))
    {
        if ('\0' != user_room->history[line_num][0] )
            i++;
        line_num = (line_num - 1 + HISTORY_SIZE) % HISTORY_SIZE;
    }
    line_num = (line_num + 1 + HISTORY_SIZE) % HISTORY_SIZE;
    sprintf(header, "%s %d %s \n","--- Chatroom History ( last ", i , " lines) ---");

    write_client( user_submitter->connection, header);
    // Print out each of the non blank lines, going forwards
    while ((line_num != user_room->history_count)&&( i > 0 ))
    {
        if ( '\0' != user_room->history[line_num][0] )  /* Skip-a da blanks */
        {
            write_client( user_submitter->connection, "%s \n",user_room->history[line_num]);
            i--;
        }
        line_num = (line_num + 1) % HISTORY_SIZE;
    }
    return SUCCESS;
}


int reset_user(user_t *user_submitter )
{
    user_submitter->admin = false;
    user_submitter->used = false;
    strcpy( user_submitter->user_name, "" );
    remove_user_from_chatroom( user_submitter );

    return true;
}

bool block_is_active( blocked_ip_t *block )
{
    return block->active != 0 ? true : false;
}

int open_blocks()
{
    int i;
    int cnt = MAX_BLOCKED;
    for(i = 0; i < MAX_BLOCKED; i++){
        if(blocks[i].active == 1){
            cnt--;
        }
    }
    return cnt;
}

int add_user_to_block_list(user_t *user_submitter, char *reason)
{
    int i;

    //Find open spot to block
    for(i = 0; i < MAX_BLOCKED; i++){
        if( !block_is_active( &blocks[i] ) ){
            blocks[i].active = 1;
            blocks[i].id = i;
            blocks[i].user_ip_addr = user_submitter->user_ip_addr;
            strcpy( blocks[i].user_name, user_submitter->user_name);
            strcpy( blocks[i].reason, reason);
            return open_blocks();
        }
    }
    return FAILURE;
}

int remove_block_list_id(user_t *user_submitter, int id_num)
{
    int i;
    for(i = 0; i < MAX_BLOCKED; i++){
        if ( blocks[i].id == id_num ){
            blocks[i].active = 0;
            write_client( user_submitter->connection, "Unblocked: %s @ %s \n", blocks[i].user_name, inet_ntoa( blocks[i].user_ip_addr ));
            return SUCCESS;
        }
    }
    write_client( user_submitter->connection, "Could not find ID %d \n", id_num);
    return FAILURE;
}

int block_user_ip( user_t *user_submitter, int argc, char **argv )
{
    int i, open_spots;
    char *user_name = argv[ 1 ];

    // verify a user name was provided and the number of args is correct
    if( user_name == NULL || 2 > argc)
    {
        return DISPLAY_USAGE;
    }

    char *block_reason;
    if(argc == 2)
    {
        // No reason was given
        block_reason = "*No reason given*";
    }
    else
    {
        // Grab the full message string from user and chop off first 2 parameters
        int offset = strlen(argv[0]) + 1 + strlen(argv[1]);
        block_reason = strstr(user_submitter->user_msg + offset, argv[2]);
    }

    // iterate through all users to find the one targeted for kick
    for( i = 0; i < MAX_CONN; i++ )
    {
        if( strcmp( user_thread[ i ].user_name, user_name ) == 0 )
        {
            // Block the targeted user
            open_spots = add_user_to_block_list( &user_thread[ i ], block_reason );


            if( open_spots >= 0 )
            {
                // Inform the user why they have been blocked then kick them
                write_client( user_thread[ i ].connection, "You have been blocked. Reason: %s \n", block_reason );
                logout( &user_thread[ i ], argc, argv );

                write_client( user_submitter->connection, "User %s was blocked. \n", user_name );
                write_client( user_submitter->connection, "%d blocks available out of %d.\n", open_spots, MAX_BLOCKED);
                return SUCCESS;
            }
            else {
                write_client( user_submitter->connection, "Could not block %s. \n", user_name );
                write_client( user_submitter->connection, "%d blocks available out of %d.\n", 0, MAX_BLOCKED);
                return FAILURE;
            }
        }
    }

    // send message to user_submitter and return targeted user was not found
    write_client( user_submitter->connection, "Could not find %s. \n", user_name );

    return FAILURE;
}

int unblock_user_ip( user_t *user_submitter, int argc, char **argv )
{
    char *id_str = argv[ 1 ];

    // verify a user name was provided and the number of args is correct
    if( id_str == NULL || 2 > argc)
    {
        return DISPLAY_USAGE;
    }

    // Convert to int
    int target_id = atoi(id_str);

    return remove_block_list_id(user_submitter, target_id);
}

int list_blocked_users( user_t *user_submitter, int argc, char **argv )
{
    bool first_line = true;
    int i;

    for( i = 0; i < MAX_BLOCKED; i++ )
    {
        if ( block_is_active( &blocks[i] ) )
        {
            // Print the header
            if ( true == first_line )
            {
                first_line = false;
                write_client( user_submitter->connection, "--- All Blocked Users --- \n" );
                write_client(user_submitter->connection, "%2s: %-10s | %15s | %s \n",
                                    "ID", "User Name", "User IP Address", "Reason");
            }

            // List each person
            write_client(user_submitter->connection, "%2d: %-10s | %15s | %s \n",
                    blocks[i].id, blocks[i].user_name, inet_ntoa( blocks[i].user_ip_addr ), blocks[i].reason);
        }
    }

    // No blocks where found
    if ( true == first_line ){
        write_client( user_submitter->connection, "--- All Blocked Users --- \nNone\n" );
    }

    return SUCCESS;
}

// String Case-Insensitive Comparison courtesy of
// http://stackoverflow.com/questions/5820810/case-insensitive-string-comp-in-c
int strcicmp( char const *a, char const *b )
{
    int d;

    for( ;; a++, b++ )
    {
        d = tolower( *a ) - tolower( *b );
        if( d != 0 || ! *a )
            break;
    }

    return d;
}

