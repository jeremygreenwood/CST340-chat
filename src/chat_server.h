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
#define MAX_ROOMS           5
#define MAX_CONN            10
#define ECHO_PORT           3456
#define MAX_ARGS            16
#define MAX_ARG_LEN         64
#define MAX_USER_NAME_LEN   32
#define MAX_ROOM_NAME_LEN   32
#define MAX_USERS_IN_ROOM   MAX_CONN
#define BUFFER_SIZE         1024
#define DFLT_CHATROOM_NAME  "lobby"
#define ADMIN_NAME          "Admin"             /*  Admin username  */
#define ADMIN_PASSWORD      "notPassword"       /*  password for admin login */


// comment/uncomment DEBUG_* to enable print debugging
//#define DEBUG_CMD

// command IDs
#define CMD_SIG             "/"

#define CMD_LOGOUT          "logout"

#define CMD_LIST_ROOMS      "listchatrooms"
#define CMD_CREATE_ROOM     "createchatroom"
#define CMD_JOIN_ROOM       "joinchatroom"
#define CMD_LEAVE_ROOM      "leavechatroom"     /* this should be a wrapper to join default chatroom */

#define CMD_LIST_ROOM_USERS "list"
#define CMD_LIST_ALL_USERS  "listall"

#define CMD_MUTE            "mute"
#define CMD_UNMUTE          "unmute"

#define CMD_WHISPER         "whisper"
#define CMD_REPLY           "reply"

#define CMD_HISTORY         "history"           /* get the history for the user's current chatroom */


// types
typedef struct user_t
{
    int                 user_id;
    char                user_name[ MAX_USER_NAME_LEN ];
    struct chat_room_t *chat_room;
    struct user_t      *reply_user;                  /* reference to user who whispered to this user */
    bool                admin;
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
    char           history[ BUFFER_SIZE ];
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

void init_chatroom( chat_room_t *room, int id, char *name );
void write_chatroom( user_t *user, char *msg, ... );
bool chatroom_is_active( chat_room_t *room );

// user command functionality
bool list_chat_rooms( user_t *user_submitter );
bool create_chat_room( user_t *user_submitter, char *new_name );
bool join_chat_room( user_t *user_submitter, char *room_name );
bool leave_chat_room( user_t *user_submitter );

bool list_chat_room_users( user_t *user_submitter );
bool list_all_users( user_t *user_submitter );

bool mute_user( user_t *user_submitter, char *name_to_mute );
bool unmute_user( user_t *user_submitter, char *name_to_mute );

bool whisper_user( user_t *user_submitter, char *user_name, char *msg );
bool reply_user( user_t *user_submitter, char *msg );

bool get_history( user_t *user_submitter, int num_lines );

// admin command functionality
bool admin_check(user_t *user_submitter);
bool chat_all( user_t *user_submitter, char *msg );

bool kick_user( user_t *user_submitter, char *name_to_logout );
bool kick_all_users_in_chat_room( user_t *user_submitter, char *name_to_logout );

bool block_user_ip( user_t *user_submitter, char *name_to_block );
bool unblock_user_ip( user_t *user_submitter, char *name_to_unblock );
bool list_blocked_users( user_t *user_submitter );



#endif /* CHAT_SERVER_H_ */
