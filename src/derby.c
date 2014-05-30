#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <fcntl.h>

#include "lib.h"
#include "types.h"

/*state variables*/
volatile sig_atomic_t work = 1;
volatile sig_atomic_t run = 0;
/*gloabal list variable*/

/*current run*/
struct horse *horses[RUN_HORSES] = { 0 };
/*previous run*/
struct horse *prev_run[RUN_HORSES] = { 0 };

// ==================
// COMMUNICATION DATA
// ==================

char *req[CLI_FUNC] = { "log", "wth", "pay", "sqd", "rpr", "nrt", "bet", "inf" };

char *invalid_method = "invalid method, for info send a <inf> request\n";
char *info = "This is horse derby simulation service\nIn order to monitor runs and make a bets you should login:\n\
	\t<log> login\nAfter that following services will be available:\n\t<bet> <money> <horse name>: put a bet on horse\n\
	\t<pay> <money>: put money on your virtual acoount\n\t<sqd>: check squad of next run\n\t<nrt>: start time of next run\n\
	\t<inf>: get information\n\t<rpr>: to check previous run results\n\t<wth> <money>: withdrawal money from virtual account\n\nHave fun!\n\n"; 
char *log_blank = "Login could not consits of blank symbols\n";
char *log_already = "You are already logged in\n"; 
char *with_invalid = "Your account balance is less than withdrawal money\n\tor";
char *login_before = "You should login before money operations\n";
char *invalid_money_format = "Invalid money format\n";
char *invalid_bet_format = "Invalid bet format\n";
char *first_run = "There are no any past runs\n";

// ================
// SIGNALS HANDLING
// ================

void sigint(int sig) {
	work = 0;
}

void sigalarm(int sig) {
	run = 1;
}

/* define pointers to cli func */
byte req_login(const char *req, char **resp, struct user *user);
byte req_widthdrawal(const char *req, char **resp, struct user *user);
byte req_payment(const char *req, char **resp, struct user *user);
byte req_squad(const char *req, char **resp, struct user *user);
byte req_prevrun(const char *req, char **resp, struct user *user);
byte req_nextrun(const char *req, char **resp, struct user *user);
byte req_bet(const char *req, char **resp, struct user *user);
byte req_info(const char *req, char **resp, struct user *user);

byte (*requests[CLI_FUNC])(const char *, char **, struct user *) = { req_login, req_widthdrawal, req_payment,
	req_squad, req_prevrun, req_nextrun, req_bet, req_info };

// ================
// COMMAND HANDLERS
// ================

// TODO: test
byte req_login(const char *req, char **resp, struct user *user) {
	char *ptr, *_ptr;
	// check if user is logged in
	if (user->name[0]) {
		*resp = log_already;
		return LOGGED_ALREADY;
	}
	// parse input and validate login
	ptr = req;
	while(*ptr) {
		if (isalpha(*ptr) || isdigit(*ptr))
			break;
		++ptr;		
	}
	
	if (!*ptr) {
		*resp = log_blank;
		return LOGIN_IS_BLANK; 
	}

	_ptr = ptr;
	while (*_ptr)
		++_ptr;
	
	strncpy(user->name, ptr, _ptr - ptr > LOGIN ? LOGIN : _ptr - ptr);
	*resp = _malloc(32 * sizeof(char));
	snprintf(*resp, 32, "Logged in: %s\n", user->name);

	return EXIT_SUCCESS;
}

// TODO: test
byte req_widthdrawal(const char *req, char **resp, struct user *user) {
	char *ptr;
	int val;
	if (!user->name[0]) {
		*resp = login_before;
		return NOT_LOGGED; 
	}
	ptr = req;
	while (*ptr && !isdigit(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return INVALID_MONEY_FORMAT;
	}

	val = atoi(ptr);
	if (val < 0 || (unsigned int)val > user->money) {
		*resp = invalid_money_format;
		return INVALID_MONEY_FORMAT;
	}
	
	user->money -= val;
	*resp = _malloc(64 * sizeof(char));

	snprintf(*resp, 64, "You have uncredit accout with %u credits.\n", (unsigned int)val);

	return EXIT_SUCCESS;
}

// TODO: test
byte req_payment(const char *req, char **resp, struct user *user) {
	char *ptr;
	int val;
	if (!user->name[0]) {
		*resp = login_before;
		return NOT_LOGGED; 
	}
	ptr = req;
	while (*ptr && !isdigit(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return INVALID_MONEY_FORMAT;
	}

	val = atoi(ptr);
	if (val < 0) {
		*resp = invalid_money_format;
		return INVALID_MONEY_FORMAT;
	}
	
	user->money += val;
	*resp = _malloc(64 * sizeof(char));

	snprintf(*resp, 64, "You have credit account with %u credits.\n", (unsigned int)val);

	return EXIT_SUCCESS;
}

// TODO: test
byte req_squad(const char *req, char **resp, struct user *user) {
	int i;
	char *ptr;
	int n;
	*resp = _malloc((HORSE_NAME + 5) * RUN_HORSES);
	memset(*resp, 0, (HORSE_NAME + 5) * RUN_HORSES);
	ptr = *resp;
	for (i = 0; i < RUN_HORSES; ++i) {
		n = snprintf(ptr, HORSE_NAME + 4, "%d. %s\n", i, *horses[i]);
		ptr += n;	
	}
		
	return EXIT_SUCCESS;
}

// TODO: test
byte req_prevrun(const char *req, char **resp, struct user *user) {
	int i;
	char *ptr;
	int n;
	*resp = _malloc((HORSE_NAME + 5) * RUN_HORSES);
	memset(*resp, 0, (HORSE_NAME + 5) * RUN_HORSES);
	ptr = *resp;
	if (!prev_run[0]) {
		*resp = first_run;
		return FIRST_RUN;
	}	

	for (i = 0; i < RUN_HORSES; ++i) {
		n = snprintf(ptr, HORSE_NAME + 4, "%d. %s\n", i, *horses[i]);
		ptr += n;	
	}
	return EXIT_SUCCESS;
}

// TODO: test
byte req_nextrun(const char *req, char **resp, struct user *user) {
	*resp = (char *)malloc(4);	
	strncpy(*resp, "ntr", 3);
	(*resp)[3] = 0;
	return EXIT_SUCCESS;
}

// TODO: implement
byte req_bet(const char *req, char **resp, struct user *user) {
	char *ptr, *_ptr;
	int val;
	if (!user->name[0]) {
		*resp = login_before;
		return NOT_LOGGED; 
	}
	ptr = req;
	while (*ptr && !isdigit(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return INVALID_BET_FORMAT;
	}

	val = atoi(ptr);
	if (val < 0 || (unsigned int)val > user->money) {
		*resp = invalid_money_format;
		return INVALID_BET_FORMAT;
	}

	// check horse
	while (*ptr && !isalpha(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return INVALID_BET_FORMAT;
	}
	// TODO: xxxxxxxxxxxxxxxxx
	_ptr = ptr;
	while (*_ptr)
		++_ptr;
	
	user->bet = val;
	*resp = _malloc(64 * sizeof(char));

	// hardcoded
	snprintf(*resp, 64, "Bet %u credits in %s horse.\n", (unsigned int)val, "Masha");

	return EXIT_SUCCESS;
}

byte req_info(const char *req, char **resp, struct user *user) {
	*resp = info;	
	return NON_FREE;
}

/*parse input data*/
// TODO: parse request packets
// TODO: change ret :)
byte parse_request(char *buf, size_t n, char **resp, struct user *user) {
	int j, ret;
	static char method[REQ_MTHD + 1] = { 0 };

	// to avoid calculation if packet lenght is less than REQ_MTHD
	if (n < REQ_MTHD)
		goto inv_mth;

	/*delete end of line*/
	for (j = 0; j < n; ++j)
		if (buf[j] == '\r' || buf[j] == '\n')
			buf[j] = 0;
		
	strncpy(method, buf, REQ_MTHD);	
	method[REQ_MTHD] = 0; // for double security
	
	/* check if it is valid method */
	for (j = 0; j < CLI_FUNC; ++j)
		if (!strcmp(method, req[j]))
			break;
	
inv_mth:
	if (j == CLI_FUNC) {
		*resp = invalid_method;
		return INVALID_METHOD;
	}
	
	//ret = (requests[j])(buf + REQ_MTHD, resp);	
	
	byte (*fn)(const char *, char **, struct user *user);
	fn = requests[j];
	fn(buf + REQ_MTHD, resp, user);
	
	return ret;	
}

// ================================
// CLIENT THREAD & HELPER FUNCTIONS
// ================================

void init_client(struct user *dest, struct user *src) {
	dest->money = dest->bet = 0;
	dest->service = src->service;

	free(src->sockfd);
	free(src);
}

// TODO: implement client thread
void* client_thread(void *arg) {
	struct user *__user = (struct user *)arg; 
	struct user user;
	int sockfd = *(__user->sockfd), j, ret;
	size_t n;
	char buf[MAXLEN + 1];
	char *resp;
	///////////////
	init_client(&user, __user);
	///////////////
	_pthread_detach(pthread_self());	

	while (1) {
handle_conn:
		if (!run) {
			// write on client terminal
			//fprintf(stderr, "thread %u was created\n", (unsigned int)pthread_self());	
			if (!(n = read(sockfd, buf, MAXLEN))) {
				/*
				if (errno == EINTR)
					goto handle_conn;
				*/
				break;
			}
			else {
				buf[n < MAXLEN ? n : MAXLEN] = 0;
				ret = parse_request(buf, n, &resp, &user);
				// send response
				_write(sockfd, (void *)resp, strlen(resp));
				if (!ret)
					free(resp);
			}
		}
		// send current state of run, and money after
		else {	
			fprintf(stderr, "horses are running: %u\n", (unsigned int)pthread_self());	
		}
	}
	
	_close(sockfd);
	return NULL;
}

// ===============================
// HORSE THREAD & HELPER FUNCTIONS
// ===============================

void* horse_thread(void *arg);

void init_horse(struct horse *ph, const char *name, pthread_mutex_t *rm, pthread_cond_t *rc, pthread_barrier_t *rb, struct service *service) {
	strncpy(ph->name, name, HORSE_NAME);
	ph->name[HORSE_NAME] = 0;
	ph->strength = START_STRENGTH;
	ph->distance = 0;
	ph->running = 0;
	ph->mutex = rm;
	ph->cond = rc;
	ph->barrier = rb;
	ph->service = service;
	_pthread_create(&(ph->tid), NULL, horse_thread, (void *)ph);
}

// TODO: implement horse thread
void* horse_thread(void *arg) {
	struct horse *horse = (struct horse *)arg;

	_pthread_detach(pthread_self());	

start:
	_pthread_mutex_lock(horse->mutex);

	while(!horse->running) {
		_pthread_cond_wait(horse->cond, horse->mutex);
		
		fprintf(stderr, "horse: %s awaken!\n", horse->name);
	}

	_pthread_mutex_unlock(horse->mutex);

	while(horse->running) {
		fprintf(stderr, "horse: %s waiting on barrier!\n", horse->name);
		//_pthread_barrier_wait(horse->barrier);

		//TODO:  make distance
		
		fprintf(stderr, "horse: %s done!\n", horse->name);
		
		_pthread_cond_wait(horse->cond, horse->mutex);
		_pthread_mutex_unlock(horse->mutex);
	}

	// continue waiting after finishing a run	
	horse->running = 0;
	horse->distance = 0;
	goto start;

	return NULL;
}

// ==============================
// SERVER WORK & HELPER FUNCTIONS
// ==============================

// TODO: add check if 
void choose_run_horses(struct horse *horses, size_t horse_number) {
	size_t cnt;
		
}

// TODO: simulation of run
void play(pthread_cond_t *cond) {
	while(run) {
		fprintf(stderr, "main: sleeping 1 sec\n");
		sleep(1);	
		_pthread_cond_broadcast(cond);
	}
} 

// TODO: implement sync
// TODO: redone accept impementation
void server_work(int listenfd, sigset_t *sint, pthread_cond_t *cond, struct service *service) {
	pthread_t tid;
	int *iptr;
	struct sockaddr_in cliaddr;
	struct user *user;
	socklen_t clilen;

	/*for signal handling*/
	_sigprocmask(SIG_BLOCK, sint, NULL);

	// ===============
	// temporary
	// ===============
	// run = 1;	

	while (work) {
		// accepting client, while there is no run
		if (!run) {
			clilen = sizeof(cliaddr); 
			iptr = _malloc(sizeof(int));
			*iptr = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);	
			user = (struct user *)_malloc(sizeof(struct user));
			user->sockfd = iptr;
			user->service = service;
			_pthread_create(&tid, NULL, client_thread, user);		
		}
		else play(cond);
	}
}

// ====================================
// PARSE CONFIG FILE & HELPER FUNCTIONS
// ====================================

byte read_int(FILE *f, unsigned int *pval) {
	int tmp;
	size_t len;
	ssize_t read;
	char *line = NULL;

	read = getline(&line, &len, f);		
	if (read < 0)
		return GETLINE_ERROR;
		
	tmp = atoi(line);	
	free(line);
	if (tmp <= 0)
		return INVALID_FILEFORMAT;
	*pval = tmp;
	return EXIT_SUCCESS;
}

// TODO: test function
// TODO: check if could be done without counter
byte parse_conf_file(const char *file, struct horse **horses, unsigned int *hn, unsigned int *rph, pthread_mutex_t *rm, pthread_cond_t *rc, 
						pthread_barrier_t *rb, struct service *service) {
	FILE *f;
	char *canonical;
	size_t len;
	ssize_t read;
	char *line = NULL;
	int tmp, cnt = 0;
	struct horse *h;	

	canonical = canonicalize_file_name(file);
	if (canonical) {
		if (TEMP_FAILURE_RETRY(f = fopen(canonical, "r")) < 0)
			ERR("fopen");
		// file was opened
		// read first line
		tmp = read_int(f, hn);
		if (tmp) {
			free(canonical);
			return tmp;	
		}
		
		// allocate memory
		*horses = _malloc(sizeof(struct horse) * *hn);		
		h = *horses;	
		while (cnt < *hn && (read = getline(&line, &len, f)) != -1) {
			init_horse(&h[cnt], line, rm, rc, rb, service);
			++cnt;
		}
			
		// read races per hour parameter
		tmp = read_int(f, rph);
		if (tmp) {
			free(canonical);
			return tmp;	
		}

		free(canonical);
	}
	else return CANONICALIZE_ERROR;		

	return EXIT_SUCCESS;
}

// ======================
// MAIN & USAGE FUNCTIONS
// ======================

void usage(const char *name) {
	fprintf(stderr, "usage: %s <port> <config file>\n", name);
}

// TODO: clean horses at the end of programm
int main(int argc, char **argv) {
	int i, listenfd, port, ret;
	struct sockaddr_in servaddr;
	sigset_t sint, sempty;
	struct horse *horses;
	unsigned int horse_num, pperh;	
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_barrier_t barrier;		
	struct service service;

	if (argc != 3) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/*parse port*/
	port = atoi(argv[1]);
	
	/*setting a Ctrl-C*/
	_sigemptyset(&sempty);
	_sigemptyset(&sint);
	_sigaddset(&sint, SIGINT);
	_sigaddset(&sint, SIGALRM);
	_signal(SIGINT, sigint);
	_signal(SIGALRM, sigalarm);
	
	listenfd = _socket(PF_INET, SOCK_STREAM, 0);
	
	memset(&servaddr, sizeof(char), sizeof(servaddr));			

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	_bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	_listen(listenfd, LISTENQ);

	// init synchronization info
	_pthread_mutex_init(&mutex, NULL);
	_pthread_cond_init(&cond, NULL);

	ret = parse_conf_file(argv[2], &horses, &horse_num, &pperh, &mutex, &cond, &barrier, &service);

	if (ret)
		goto clean;	

	server_work(listenfd, &sint, &cond, &service);
	
	ret = EXIT_SUCCESS;

clean:
	/*closing listen file desc*/
	_close(listenfd);
	_pthread_mutex_destroy(&mutex);
	free(horses);	

	return EXIT_SUCCESS;
}
