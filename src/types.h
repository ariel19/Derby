#pragma once

#include <pthread.h>

// return codes
#define NULL_PTR				1
#define CANONICALIZE_ERROR		2
#define GETLINE_ERROR			3
#define INVALID_FILEFORMAT		4
#define INVALID_METHOD			5
#define NON_FREE				6
#define LOGGED_ALREADY			7
#define LOGIN_IS_BLANK			8
#define NOT_LOGGED				9
#define INVALID_MONEY_FORMAT 	10
#define INVALID_BET_FORMAT		11
#define FIRST_RUN				12
#define ABSENT_HORSE			13

// predefined values
#define HORSE_NAME 16
#define LOGIN	16
#define START_STRENGTH 100
#define LISTENQ	  	1024	  
#define MAXLEN		32
#define CLI_FUNC	8
#define REQ_MTHD	3
#define RUN_HORSES	8
#define DISTANCE	100

typedef unsigned char byte;

struct service {
	/*last win horse*/
	struct horse *win;
	/*previous run*/
	//struct horse *prev_run[RUN_HORSES];
	/*bank*/
	unsigned int bank;
	pthread_mutex_t *mbank;
	/*current run positions*/
	struct horse *current_run[RUN_HORSES];	
	/*delay before next race*/
	unsigned int delay;
	/*next race time*/
	/*number of connected clients*/
	unsigned int clients;
	/*is run finished*/
	byte finished;
	pthread_mutex_t *mfinished;
	/*running horses*/
	unsigned int cur_run;
	pthread_mutex_t *mcur_run;
};

struct horse {
	char name[HORSE_NAME + 1] ;
	int strength;
	unsigned int distance;
	byte running;
	pthread_t tid;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
	pthread_barrier_t *barrier;
	struct service *service;
};

// TODO: fullfill structure
struct user {
	char name[LOGIN + 1];
	int money;
	int bet;
	int *sockfd;
	struct service *service;
	struct horse *horse;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
};

struct list_user {
	struct user_node *head; 
	struct user_node *tail;
};

struct user_node {
	struct user *user;
	struct user_node *next;
};
