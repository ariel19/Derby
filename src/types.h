#pragma once

#include <pthread.h>

// ===================
// FILE PARSING ERRORS
// ===================

#define FILE_CANONICALIZE_ERROR		0xf0
#define FILE_GETLINE_ERROR			0xf1
#define FILE_INVALID_FORMAT			0xf2

// ==============
// SERVICE ERRORS
// ==============

#define SERVICE_INVALID_METHOD		0x50
#define SERVICE_NON_FREE			0x51
#define SERVICE_LOGIN_ALREADY		0x52
#define SERVICE_LOGIN_IS_BLANK		0x53
#define SERVICE_LOGIN_NONE			0x54
#define SERVICE_INVALID_MONEY_FORMAT 0x55
#define SERVICE_INVALID_BET_FORMAT	0x56
#define SERVICE_RACE_FIRST_RUN		0x57
#define SERVICE_RACE_ABSENT_HORSE	0x58

// ==========
// HORSE DATA
// ==========

#define HORSE_NAME 				16
#define HORSE_START_STRENGTH 	100
#define HORSE_RUN				8
#define TRACK_DISTANCE			100

// =========
// USER DATA
// =========

#define USER_LOGIN_LENGTH		16

// ===========
// SERVER DATA
// ===========

#define LISTENQ	  				1024	  
#define MSG_MAXLEN				32
#define CLI_FUNC				8
#define REQ_MTHD				3
#define MAX_RACE_NUM			60

typedef unsigned char byte;

struct service {
	// last winner
	struct horse *win;
	// bank money
	unsigned int bank;
	pthread_mutex_t *mbank;
	// array of currently running horsers
	struct horse *current_run[HORSE_RUN];	
	// mutex on i'th horse
	pthread_mutex_t *mhb;
	// bet for i'th horse
	unsigned int horse_bet[HORSE_RUN];
	// sending money to clients info
	unsigned int copy_horse_bet[HORSE_RUN];
	// delay before next race
	unsigned int delay;
	// next race time
	time_t next_race;
	// mutex on next_race time
	pthread_mutex_t *mnr; 
	// number of connected clients
	unsigned int clients;
	// is run finished
	byte finished;
	// mutex on finished value
	pthread_mutex_t *mfinished;
	// number of currently running horses
	unsigned int cur_run;
	// mutex on number of currently running horses 
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

struct user {
	char name[USER_LOGIN_LENGTH + 1];
	int money;
	int bet;
	int *sockfd;
	struct service *service;
	struct horse *horse;
	int id;
	pthread_mutex_t *mhb;
	pthread_mutex_t *mnr;
};
