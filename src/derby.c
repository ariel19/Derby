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
char *horse_absent = "Horse is absent\n";

// ================
// SIGNALS HANDLING
// ================

void sigint(int sig) {
	work = 0;
}

void sigalarm(int sig) {
	run = 1;
}

// ===============
// SIGNAL BLOCKING
// ===============
void t_sigmask(int sig, int how) {
	sigset_t set;
	
	_sigemptyset(&set);
	_sigaddset(&set, sig);
	_pthread_sigmask(how, &set, NULL);
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

byte req_squad(const char *req, char **resp, struct user *user) {
	int i;
	char *ptr;
	int n;
	n = (HORSE_NAME + 5) * RUN_HORSES;
	*resp = _malloc(n);
	memset(*resp, 0, n);
	ptr = *resp;
	for (i = 0; i < RUN_HORSES; ++i) {
		n = snprintf(ptr, HORSE_NAME + 4, "%d. %s\n", i, user->service->current_run[i]->name);
		ptr += n;	
	}
	
	return EXIT_SUCCESS;
}

byte req_prevrun(const char *req, char **resp, struct user *user) {
	int i;
	char *ptr;
	int n;
	*resp = _malloc(HORSE_NAME + 2);
	memset(*resp, 0, HORSE_NAME + 2);	
	if (!user->service->win) {
		*resp = first_run;
		return FIRST_RUN;
	}
	strncpy(*resp, user->service->win->name, HORSE_NAME);
	(*resp)[strlen(*resp)] = '\n';	

	return EXIT_SUCCESS;
}

// TODO: implement
byte req_nextrun(const char *req, char **resp, struct user *user) {
	*resp = (char *)malloc(4);	
	strncpy(*resp, "ntr", 3);
	(*resp)[3] = 0;
	return EXIT_SUCCESS;
}

// TODO: implement
// TODO: test
byte req_bet(const char *req, char **resp, struct user *user) {
	char *ptr, *_ptr;
	int val, i;
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
	_ptr = ptr;
	while (*_ptr)
		++_ptr;
	
	user->bet = val;
	*resp = _malloc(64 * sizeof(char));

	// search for horse in currently running
	for (i = 0; i < RUN_HORSES; ++i) {
		if (!strcmp(ptr, user->service->current_run[i]->name)) {
			user->horse = user->service->current_run[i];
			break;
		}
	}
	
	if (i == RUN_HORSES) {
		*resp = horse_absent;
		return ABSENT_HORSE;
	}

	// add money to bank accout
	_pthread_mutex_lock(user->service->mbank);
	user->service->bank += (unsigned int)val;
	_pthread_mutex_unlock(user->service->mbank);
	
	snprintf(*resp, 64, "Bet %u credits on %s horse.\n", (unsigned int)val, ptr);

	return EXIT_SUCCESS;
}

byte req_info(const char *req, char **resp, struct user *user) {
	*resp = info;	
	return NON_FREE;
}

/*parse input data*/
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
	
	return fn(buf + REQ_MTHD, resp, user);
}

// ================================
// CLIENT THREAD & HELPER FUNCTIONS
// ================================

void init_client(struct user *dest, struct user *src) {
	memset(dest->name, 0, HORSE_NAME + 1);
	dest->money = dest->bet = 0;
	dest->service = src->service;
	free(src->sockfd);
	free(src);
}

// TODO: send data about running horses	
// TODO: send money at the end of race
void* client_thread(void *arg) {
	struct user *__user = (struct user *)arg; 
	struct user user;
	int sockfd = *(__user->sockfd), j, ret;
	size_t n;
	char buf[MAXLEN + 1];
	char *resp;
	init_client(&user, __user);
	_pthread_detach(pthread_self());	

	while (1) {
handle_conn:
		if (!run) {
			// write on client terminal
			if (!(n = read(sockfd, buf, MAXLEN))) {
				if (errno == EINTR)
					goto handle_conn;
				break;
			}
			else {
				buf[n < MAXLEN ? n : MAXLEN] = 0;
				ret = parse_request(buf, n, &resp, &user);
				fprintf(stderr, "RET: %d\n", ret);
				// send response
				_write(sockfd, (void *)resp, strlen(resp));
				if (!ret)
					free(resp);
			}
		}
		// send current state of run, and money after
		else {
			// send data about running horses	
			// fprintf(stderr, "horses are running: %u\n", (unsigned int)pthread_self());	
			// put a conditional here
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
	if (ph->name[strlen(ph->name) - 1] == 0xa)
		ph->name[strlen(ph->name) - 1] = 0;
	ph->strength = START_STRENGTH;
	ph->distance = 0;
	ph->running = 0;
	ph->mutex = rm;
	ph->cond = rc;
	ph->barrier = rb;
	ph->service = service;
	_pthread_create(&(ph->tid), NULL, horse_thread, (void *)ph);
}

unsigned int horse_make_step() {
	return rand() % 30;
}

// TODO: implement horse thread
// TODO: add win mutex
void* horse_thread(void *arg) {
	struct horse *horse = (struct horse *)arg;
	struct service *service = horse->service;

	_pthread_detach(pthread_self());	

start:
	_pthread_mutex_lock(horse->mutex);

	while(!horse->running) {
		_pthread_cond_wait(horse->cond, horse->mutex);
		
		//fprintf(stderr, "horse: %s awaken!\n", horse->name);
	}

	_pthread_mutex_unlock(horse->mutex);

	while(horse->running) {
		//fprintf(stderr, "horse: %s waiting on barrier!\n", horse->name);
		//_pthread_barrier_wait(horse->barrier);

		_pthread_mutex_lock(service->mfinished);
		if (service->finished) {
			_pthread_mutex_unlock(service->mfinished);
			break;
		}
		
		_pthread_mutex_unlock(service->mfinished);

		// make step	
		horse->distance += horse_make_step();
		fprintf(stderr, "horse %s: %u distance\n", horse->name, horse->distance);	
		
		// means that horse has finished a track
		if (horse->distance >= DISTANCE) {
			fprintf(stderr, "horse %s: got a distance\n", horse->name);
			_pthread_mutex_lock(service->mfinished);
			if (service->finished) {
				_pthread_mutex_unlock(service->mfinished);
				break;
			}
			fprintf(stderr, "horse %s: WINS\n", horse->name);	
			service->finished = 1;
			service->win = horse;

			_pthread_mutex_unlock(service->mfinished);
			break;
		}	

		
		//fprintf(stderr, "horse: %s done!\n", horse->name);
		
		_pthread_cond_wait(horse->cond, horse->mutex);
		_pthread_mutex_unlock(horse->mutex);
	}

	// continue waiting after finishing a run	
	fprintf(stderr, "horse %s had finished\n", horse->name);
	
	horse->running = 0;
	horse->distance = 0;

	//fprintf(stderr, "BLOCKING MUTEX: HORSE\n");
	//fflush(stderr);	
	_pthread_mutex_lock(service->mcur_run);
	--service->cur_run;
	fprintf(stderr, "cur_run %d\n", service->cur_run);
	_pthread_mutex_unlock(service->mcur_run);
	//fprintf(stderr, "UNBLOCKING MUTEX: HORSE\n");
	//fflush(stderr);	

	goto start;

	return NULL;
}

// ==============================
// SERVER WORK & HELPER FUNCTIONS
// ==============================

// TODO: implement
// TODO: add mutex locking (acces to field is finished)
void choose_run_horses(struct horse *horses, size_t horse_number, struct service *service) {
	size_t cnt = 0;
	int i = 0;
	if (horse_number < RUN_HORSES) {
		fprintf(stderr, "Current number of horses is %d, should be at least %d\n", horse_number, RUN_HORSES);
		exit(EXIT_FAILURE);
	}
	// set all running	
	if (horse_number == RUN_HORSES) {
		for (i = 0; i < RUN_HORSES; ++i) {
			//horses[i].running = 1;
			service->current_run[i] = &horses[i];	
		}
		return;
	}

	// choose randomly
	while(cnt != RUN_HORSES) {	
		i = rand() % horse_number;
		if (!horses[i].running) {
			//horses[i].running = 1;
			service->current_run[cnt++] = &horses[i];
		}
	}
}

void start_horses(struct service *service) {
	int i;
	for (i = 0; i < RUN_HORSES; ++i)
		service->current_run[i]->running = 1;
}

// TODO: simulation of run
// TODO: set alarm
void play(pthread_cond_t *cond, struct service *service, struct horse *horses, size_t horse_num) {
	int i;
	start_horses(service);
	while(run) {
		_pthread_mutex_lock(service->mfinished);
		if (service->finished) {
			_pthread_mutex_unlock(service->mfinished);

			// send if someone is waiting
			_pthread_cond_broadcast(cond);

			fprintf(stderr, "Waiting for a horse threads finishing\n");
check_for_horses:
			_pthread_mutex_lock(service->mcur_run);
			if (!service->cur_run) {

				_pthread_mutex_unlock(service->mcur_run);	
				break;
			}
			else {
				_pthread_mutex_unlock(service->mcur_run);
				sleep(1);
				goto check_for_horses;
			}
		}
		_pthread_mutex_unlock(service->mfinished);	
		fprintf(stderr, "=========================\nmain: sleeping 1 sec\n===============================\n");
		sleep(1);	
		_pthread_cond_broadcast(cond);
	}
	_pthread_mutex_lock(service->mfinished);
	service->finished = 0;
	_pthread_mutex_unlock(service->mfinished);

	_pthread_mutex_lock(service->mcur_run);
	service->cur_run = RUN_HORSES;
	_pthread_mutex_unlock(service->mcur_run);

	/*
	for (i = 0; i < RUN_HORSES; ++i)
		service->prev_run[i] = service->current_run[i];
	*/
	// choose new horses
	choose_run_horses(horses, horse_num, service);	

	run = 0;
	alarm(5);	
} 

// TODO: implement sync
// TODO: redone accept impementation
// TODO: alarm!!!!!!!!!!!!!
void server_work(int listenfd, sigset_t *sint, pthread_cond_t *cond, struct service *service, struct horse *horses, size_t horse_num) {
	pthread_t tid;
	int *iptr;
	struct sockaddr_in cliaddr;
	struct user *user;
	socklen_t clilen;

	/*for signal handling*/
	
	t_sigmask(SIGINT, SIG_UNBLOCK);
	t_sigmask(SIGALRM, SIG_UNBLOCK);	
	
	// set alarm for next run
	choose_run_horses(horses, horse_num, service);
	//alarm(service->delay);

	// ----
	// TEST
	// ----
	// run = 1;
	//alarm(20);

	while (work) {
		// accepting client, while there is no run
		if (!run) {
			clilen = sizeof(cliaddr); 
			iptr = _malloc(sizeof(int));
			*iptr = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);	
			if (run) {
				free(iptr);
				fprintf(stderr, "Continue\n");
				fflush(stderr);
				continue;
			}
			user = (struct user *)_malloc(sizeof(struct user));
			user->sockfd = iptr;
			user->service = service;
			_pthread_create(&tid, NULL, client_thread, user);		
		}
		else play(cond, service, horses, horse_num);
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

// =================
// SERVICE FUNCTIONS
// =================
void init_service(struct service *service, pthread_mutex_t *mf, pthread_mutex_t *mb, pthread_mutex_t *mcr, unsigned int delay) {
	service->mfinished = mf;
	service->mbank = mb;
	service->bank = 0;
	service->finished = 0;
	service->mcur_run = mcr;
	service->cur_run = RUN_HORSES;
	service->delay =  delay;
	// init first elemet of prev run with 0 in order to distinguish if run before current has occured
	// service->prev_run[0] = NULL;
	service->win = NULL;
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
	pthread_mutex_t mutex, mfinished, mbank, mcur_run;
	pthread_cond_t cond;
	pthread_barrier_t barrier;		
	struct service service;

	if (argc != 3) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/*parse port*/
	port = atoi(argv[1]);

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
	
	if (!pperh || pperh > 60) {
		fprintf(stderr, "races per hour %u should be more than 0 and less than 61\n", pperh);
		exit(EXIT_FAILURE);
	}

	if (ret)
		goto clean;	

	_signal(SIGINT, sigint);
	_signal(SIGALRM, sigalarm);
	
	t_sigmask(SIGINT, SIG_BLOCK);
	t_sigmask(SIGALRM, SIG_BLOCK);	
	
	srand(time(NULL));	
	
	// service info
	_pthread_mutex_init(&mfinished, NULL);
	_pthread_mutex_init(&mbank, NULL);
	_pthread_mutex_init(&mcur_run, NULL);

	init_service(&service, &mfinished, &mbank, &mcur_run, (unsigned int)(60 * 60 / pperh));	

	server_work(listenfd, &sint, &cond, &service, horses, horse_num);
	
	ret = EXIT_SUCCESS;

clean:
	/*closing listen file desc*/
	fprintf(stderr, "Start cleaning...\n");
	fflush(stderr);
	_close(listenfd);
	fprintf(stderr, "GLobal mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(&mutex);
	fprintf(stderr, "Finished mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(&mfinished);
	fprintf(stderr, "Bank mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(&mbank);
	fprintf(stderr, "Current run mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(&mcur_run);
	free(horses);	

	return EXIT_SUCCESS;
}
