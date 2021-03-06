/*===========================================================================
 Filename    : chat_server.h
 Authors     : Jeremy Greenwood <jeremy.greenwood@oit.edu>,
             : Joshua Durkee    <joshua.durkee@oit.edu>
 Course      : CST 340
 Assignment  : 6
 Description : Collaborative multi-threaded chat server. Project located at
               https://github.com/jeremygreenwood/CST340-chat
===========================================================================*/

#ifndef CHAT_SERVER_H_
#define CHAT_SERVER_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>     /*  socket definitions        */
#include <sys/types.h>      /*  socket types              */
#include <arpa/inet.h>      /*  inet (3) funtions         */
#include <unistd.h>         /*  misc. UNIX functions      */
#include <pthread.h>
#include <semaphore.h>
#include <time.h>           /*  time functions            */
#include <ctype.h>          /*  for tolower() function    */
#include "helper.h"         /*  our own helper functions  */


// constants
#define SUCCESS             0
#define FAILURE             ( -1 )
#define DISPLAY_USAGE       ( -2 )
#define NOT_ADMIN           ( -3 )
#define MAX_ROOMS           5
#define MAX_CONN            10
#define MAX_BLOCKED         2
#define ECHO_PORT           3456
#define MAX_ARGS            16
#define MAX_ARG_LEN         64
#define MAX_CMD_STR_LEN     32
#define MAX_CMD_USAGE_LEN   512
#define MAX_USER_NAME_LEN   32                  /* maximum characters including null terminating character */
#define MAX_ROOM_NAME_LEN   32                  /* maximum characters including null terminating character */
#define MAX_USERS_IN_ROOM   MAX_CONN
#define HISTORY_SIZE        50                  /* max lines of history */
#define BUFFER_SIZE         1024                /* max length of message */
#define TIMESTAMP_SIZE      20                  /* length of timestamp ddd HH:MM:SS PM */
#define DFLT_CHATROOM_NAME  "lobby"
#define ADMIN_NAME          "Admin"             /*  Admin username  */
#define ADMIN_PASSWORD      "notPassword"       /*  password for admin login */


// comment/uncomment DEBUG_* to enable print debugging
//#define DEBUG_CMD

// command IDs
#define CMD_SIG             "/"

#define CMD_HELP            "help"              /* display a list of all valid commands */

#define CMD_LOGOUT          "logout"

#define CMD_LIST_ROOMS      "listchatrooms"
#define CMD_CREATE_ROOM     "createchatroom"
#define CMD_JOIN_ROOM       "joinchatroom"
#define CMD_LEAVE_ROOM      "leavechatroom"     /* this should be a wrapper to join default chatroom */
#define CMD_WHERE_AM_I      "whereami"          /* command for user to query which room they are in */

#define CMD_LIST_ROOM_USERS "list"
#define CMD_LIST_ALL_USERS  "listall"

#define CMD_MUTE            "mute"
#define CMD_UNMUTE          "unmute"

#define CMD_WHISPER         "whisper"
#define CMD_REPLY           "reply"

#define CMD_HISTORY         "history"           /* get the history for the user's current chatroom */

#define CMD_KICK            "kick"
#define CMD_KICK_ALL        "kickall"

#define CMD_BLOCK           "block"
#define CMD_UNBLOCK         "unblock"
#define CMD_LISTBLOCK       "listblock"

#define CMD_CHAT_ALL        "broadcast"         /* send a message to all logged-in users            */

#define SLASH_VALUE         '/'


// types
typedef struct user_t
{
    int                 user_id;
    char                user_name[ MAX_USER_NAME_LEN ];
    struct chat_room_t *chat_room;                  /* Name of chatroom user is currently in           */
    struct user_t      *reply_user;                 /* reference to user who whispered to this user    */
    char                muted_users[ MAX_CONN ][ MAX_USER_NAME_LEN ];    /* users muted by this user   */
    bool                admin;                      /* Whether user is administrative user             */
    bool                login_failure;              /* signifies an invalid password was used to logon */
    int                 connection;                 /* socket file descriptor */
    pthread_t           thread;
    bool                used;                       /* Whether user struct is used/contains user data  */
    bool                logout;                     /* Whether user has logged out                     */
    sem_t               write_mutex;                /* write lock for client's connection */
    char                user_msg[ BUFFER_SIZE ];    /* last message sent from this user */
    struct in_addr      user_ip_addr;               /* IP address user is connected from */
} user_t;

// Struct for storing lines of history so we can apply mutes to history
typedef struct history_line_t
{
    char                message[BUFFER_SIZE];           /* line to be logged */
    char                timestamp[TIMESTAMP_SIZE];      /* when message was sent */
	char                user_name[MAX_USER_NAME_LEN];   /* user who sent the message */
} history_line_t;


typedef struct chat_room_t
{
    int            room_id;
    char           room_name[ MAX_ROOM_NAME_LEN ];
    int            user_count;
    struct user_t *users[ MAX_USERS_IN_ROOM ];
    struct history_line_t history[HISTORY_SIZE];   /* Chat room's chat history */
    sem_t          history_mutex;  /* For avoiding history collisions */
    int            history_count;  /* Points to next available history line */
} chat_room_t;

// Struct for storing that an IP was blocked by an administrator
// Blocked users cannot connect to the server
typedef struct blocked_ip_t
{
    struct in_addr      user_ip_addr;                       /* IP address that was blocked */
    char                user_name[ MAX_USER_NAME_LEN ];     /* user that was blocked       */
    char                reason[ BUFFER_SIZE ];              /* Reason user was blocked     */
    int                 active;                         
    int                 id;
} blocked_ip_t;


// prototypes
void *user_proc( void *arg );
void process_client_msg( user_t *user, char *chat_msg );
int get_command( char *msg, char **argv );
void process_command( user_t *user, int argc, char **argv );
void write_all_clients( char *msg, ... );
void server_error( char *msg );
void init_user_thread( void );
void destroy_user_thread( void );
void get_username( user_t *user );
bool admin_check( user_t *user_submitter );
int reset_user( user_t *user_submitter );   /* Clear all values from user struct so it's ready to be re-used */
bool is_logged_in( char *user_name, user_t **user_pointer );      /* Get reference to user logged in with given name */
bool is_ignoring_user_name( user_t *user_ignoring, char *ignore_name ); /* Determine if given user is ignoring a name */
void print_mute_list( user_t *user_submitter );  /* Print the mute list for given user */
// String Case-Insensitive Comparison courtesy of
// http://stackoverflow.com/questions/5820810/case-insensitive-string-comp-in-c
int strcicmp( char const *a, char const *b );



// chatroom helper functions
void init_chatroom( chat_room_t *room, int id, char *name );
void write_chatroom( user_t *user, char *msg, ... );
void write_chatroom_history( user_t *user, char *message ); /* write message to user's current room's history */
bool is_valid_history_line(user_t *user_submitter, int line_num); /* indicate whether user should see give line of room's history */
bool chatroom_is_active( chat_room_t *room );
int add_user_to_chatroom( user_t *user, chat_room_t *room );
int remove_user_from_chatroom( user_t *user );


// ********** COMMANDS *************
// Note: when returning DISPLAY_USAGE status, usage statement will be printed by process_command() function

// user command functionality
int help( user_t *user_submitter, int argc, char **argv );

int logout( user_t *user_submitter, int argc, char **argv );

int list_chat_rooms( user_t *user_submitter, int argc, char **argv );
int create_chat_room( user_t *user_submitter, int argc, char **argv );
int join_chat_room( user_t *user_submitter, int argc, char **argv );
int leave_chat_room( user_t *user_submitter, int argc, char **argv );
int where_am_i( user_t *user_submitter, int argc, char **argv );

int list_chat_room_users( user_t *user_submitter, int argc, char **argv );
int list_all_users( user_t *user_submitter, int argc, char **argv );

int mute_user( user_t *user_submitter, int argc, char **argv );
int unmute_user( user_t *user_submitter, int argc, char **argv );

int whisper_user( user_t *user_submitter, int argc, char **argv );
int reply_user( user_t *user_submitter, int argc, char **argv );

int get_history( user_t *user_submitter, int argc, char **argv );

// admin command functionality
int chat_all( user_t *user_submitter, int argc, char **argv );

int kick_user( user_t *user_submitter, int argc, char **argv );
int kick_all_users_in_chat_room( user_t *user_submitter, int argc, char **argv );

int block_user_ip( user_t *user_submitter, int argc, char **argv );
int unblock_user_ip( user_t *user_submitter, int argc, char **argv );
int list_blocked_users( user_t *user_submitter, int argc, char **argv );


typedef struct command_t
{
    char        command_string[ MAX_CMD_STR_LEN ];
    int         (*command_function) ( user_t *user, int argc, char **argv );
    char        command_parameter_usage[ MAX_CMD_USAGE_LEN ];
} command_t;


command_t   commands[] =
{
    { CMD_HELP,             help,                       "[command]"                     },
    { CMD_LOGOUT,           logout,                     ""                              },
    { CMD_LIST_ROOMS,       list_chat_rooms,            ""                              },
    { CMD_CREATE_ROOM,      create_chat_room,           "<chatroomname>"                },
    { CMD_JOIN_ROOM,        join_chat_room,             "<chatroomname>"                },
    { CMD_LEAVE_ROOM,       leave_chat_room,            ""                              },
    { CMD_LIST_ROOM_USERS,  list_chat_room_users,       ""                              },
    { CMD_LIST_ALL_USERS,   list_all_users,             ""                              },
    { CMD_WHERE_AM_I,       where_am_i,                 ""                              },
    { CMD_WHISPER,          whisper_user,               "<user> <message>"              },
    { CMD_REPLY,            reply_user,                 "<message>"                     },
    { CMD_HISTORY,          get_history,                "<lines>"                       },
    // { CMD_KICK,             kick_user,                  "<user>"                        },
    // { CMD_KICK_ALL,         kick_all_users_in_chat_room,"<chatroomname>"                },
    { CMD_MUTE,             mute_user,                  "[user]"                        },
    { CMD_UNMUTE,           unmute_user,                "[user]"                        },
    // { CMD_BLOCK,            block_user_ip,              "<user> [reason]"               },
    // { CMD_UNBLOCK,          unblock_user_ip,            "<blockID>"                     },
    // { CMD_LISTBLOCK,        list_blocked_users,         ""                              },
    // { CMD_CHAT_ALL,         chat_all,                   "<message>"                     },
};

typedef struct admin_command_t
{
    char        command_string[ MAX_CMD_STR_LEN ];
    int         (*command_function) ( user_t *user, int argc, char **argv );
    char        command_parameter_usage[ MAX_CMD_USAGE_LEN ];
} admin_command_t;


admin_command_t   admin_commands[] =
{
    { CMD_KICK,             kick_user,                  "<user>"                        },
    { CMD_KICK_ALL,         kick_all_users_in_chat_room,"<chatroomname>"                },
    { CMD_BLOCK,            block_user_ip,              "<user> [reason]"               },
    { CMD_UNBLOCK,          unblock_user_ip,            "<blockID>"                     },
    { CMD_LISTBLOCK,        list_blocked_users,         ""                              },
    { CMD_CHAT_ALL,         chat_all,                   "<message>"                     },    
};

#endif /* CHAT_SERVER_H_ */
