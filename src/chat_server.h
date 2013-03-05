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
#define MAX_CHAT_NAME_LEN   32
#define MAX_USERS_IN_ROOM   MAX_CONN
#define BUFFER_SIZE         100000


// comment/uncomment DEBUG_* to enable print debugging
//#define DEBUG_CMD

// command IDs
#define CMD_SIG             "/"
#define CMD_LOGOUT          "logout"
#define CMD_CHANGE_NAME     "changeusername"


// types
typedef struct user_t
{
    int                 user_id;
    char                user_name[ MAX_USER_NAME_LEN ];
    struct chat_room_t *chat_room;
    bool                admin;
} user_t;


typedef struct worker_t
{
    struct user_t user;
    int           connection;
    pthread_t     thread;
    bool          used;
    bool          logout;
    sem_t         write_mutex;
} worker_t;


typedef struct chat_room_t
{
    int            room_id;
    char           room_name[ MAX_CHAT_NAME_LEN ];
    int            user_count;
    struct user_t *users[ MAX_USERS_IN_ROOM ];
    char           history[ BUFFER_SIZE ];
//  bool           active;                             // is user_count > 0 the same as active?
} chat_room_t;


// prototypes
void *worker_proc( void *arg );
void process_client_msg( worker_t *worker, char *chat_msg );
int get_command( char *msg, char **argv );
void process_command( worker_t *worker, int argc, char **argv );
void write_all_clients( char *msg, ... );
void server_error( char *msg );
void init_g_worker( void );
void destroy_g_worker( void );


#endif /* CHAT_SERVER_H_ */
