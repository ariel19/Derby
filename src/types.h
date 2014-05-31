#pragma once

#include <pthread.h>

// return codes
//#define NULL_PTR					1
// file parsing errors
#define FILE_CANONICALIZE_ERROR		2
#define FILE_GETLINE_ERROR			3
#define FILE_INVALID_FORMAT			4
// service errors
#define SERVICE_INVALID_METHOD		5
#define SERVICE_NON_FREE			6
#define SERVICE_LOGIN_ALREADY		7
#define SERVICE_LOGIN_IS_BLANK		8
#define SERVICE_LOGIN_NONE			9
#define SERVICE_INVALID_MONEY_FORMAT 10
#define SERVICE_INVALID_BET_FORMAT	11
#define SERVICE_RACE_FIRST_RUN		12
#define SERVICE_RACE_ABSENT_HORSE	13

// horse
#define HORSE_NAME 16
#define HORSE_START_STRENGTH 100
#define HORSE_RUN	8
#define TRACK_DISTANCE	100
// user
#define USER_LOGIN_LENGTH	16
// service
#define LISTENQ	  	1024	  
#define MSG_MAXLEN	32
#define CLI_FUNC	8
#define REQ_MTHD	3

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
	struct horse *current_run[HORSE_RUN];	
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
	char name[USER_LOGIN_LENGTH + 1];
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
