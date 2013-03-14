/*===========================================================================
 Filename    : chat_server.h
 Authors     : Jeremy Greenwood <jeremy.greenwood@oit.edu>,
             :
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
#include "helper.h"         /*  our own helper functions  */


// constants
#define SUCCESS             0
#define FAILURE             ( -1 )
#define DISPLAY_USAGE       ( -2 )
#define NOT_ADMIN           ( -3 )
#define MAX_ROOMS           5
#define MAX_CONN            10
#define ECHO_PORT           3456
#define MAX_ARGS            16
#define MAX_ARG_LEN         64
#define MAX_CMD_STR_LEN     32
#define MAX_CMD_USAGE_LEN   512
#define MAX_USER_NAME_LEN   32
#define MAX_ROOM_NAME_LEN   32
#define MAX_USERS_IN_ROOM   MAX_CONN
#define HISTORY_SIZE        50                  /* max lines of history */
#define BUFFER_SIZE         1024                /* max length of message */
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


// types
typedef struct user_t
{
    int                 user_id;
    char                user_name[ MAX_USER_NAME_LEN ];
    struct chat_room_t *chat_room;
    struct user_t      *reply_user;                 /* reference to user who whispered to this user */
    bool                admin;
    bool                login_failure;              /* signifies an invalid password was used to logon */
    int                 connection;
    pthread_t           thread;
    bool                used;
    bool                logout;
    sem_t               write_mutex;
} user_t;


typedef struct chat_room_t
{
    int            room_id;
    char           room_name[ MAX_ROOM_NAME_LEN ];
    int            user_count;
    struct user_t *users[ MAX_USERS_IN_ROOM ];
    char           history[HISTORY_SIZE][MAX_LINE];
    sem_t          history_mutex;  /* For avoiding history collisions */
    int            history_count;  /* Points to next available history line */
} chat_room_t;


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

// chatroom helper functions
void init_chatroom( chat_room_t *room, int id, char *name );
void write_chatroom( user_t *user, char *msg, ... );
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
    { CMD_WHERE_AM_I,       where_am_i,                 ""                              },
    { CMD_WHISPER,          whisper_user,               "<user> <message>"              },
    { CMD_REPLY,            reply_user,                 "<message>"                     },
    { CMD_HISTORY,          get_history,                "<lines>"                       },
};


#endif /* CHAT_SERVER_H_ */
