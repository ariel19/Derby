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

// state variables
volatile sig_atomic_t work = 1;
volatile sig_atomic_t run = 0;

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

// ===============
// SIGNAL BLOCKING
// ===============
void t_sigmask(int sig, int how) {
	sigset_t set;
	
	_sigemptyset(&set);
	_sigaddset(&set, sig);
	_pthread_sigmask(how, &set, NULL);
}

// ================
// SIGNALS HANDLING
// ================
void sigint(int sig) {
	work = 0;
}

void sigalarm(int sig) {
	run = 1;
}

void init_signals() {
	_signal(SIGINT, sigint);
	_signal(SIGALRM, sigalarm);
	
	t_sigmask(SIGINT, SIG_BLOCK);
	t_sigmask(SIGALRM, SIG_BLOCK);	
}

// ==============
// TIME FUNCTIONS
// ==============
void set_next_race_time(struct service *service) {
	time_t now = time(NULL);
	struct tm *tm_now = localtime(&now);
	if (!tm_now)
		ERR("localtime");
	struct tm tm_then = *tm_now;
	tm_then.tm_min += service->delay;
	_pthread_mutex_lock(service->mnr);
	service->next_race = mktime(&tm_then);
	_pthread_mutex_unlock(service->mnr);				
}

char* get_next_race_time(struct service *service) {
	char *ret;
	_pthread_mutex_lock(service->mnr);
	ret = ctime(&service->next_race);
	_pthread_mutex_unlock(service->mnr);
	return ret;	
}

// define pointers to cli func
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
		return SERVICE_LOGIN_ALREADY;
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
		return SERVICE_LOGIN_IS_BLANK; 
	}

	_ptr = ptr;
	while (*_ptr)
		++_ptr;
	
	strncpy(user->name, ptr, _ptr - ptr > USER_LOGIN_LENGTH ? USER_LOGIN_LENGTH : _ptr - ptr);
	*resp = _malloc(32 * sizeof(char));
	snprintf(*resp, 32, "Logged in: %s\n", user->name);

	return EXIT_SUCCESS;
}

byte req_widthdrawal(const char *req, char **resp, struct user *user) {
	char *ptr;
	int val;
	if (!user->name[0]) {
		*resp = login_before;
		return SERVICE_LOGIN_NONE; 
	}
	ptr = req;
	while (*ptr && !isdigit(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return SERVICE_INVALID_MONEY_FORMAT;
	}

	val = atoi(ptr);
	if (val < 0 || (unsigned int)val > user->money) {
		*resp = invalid_money_format;
		return SERVICE_INVALID_MONEY_FORMAT;
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
		return SERVICE_LOGIN_NONE; 
	}
	ptr = req;
	while (*ptr && !isdigit(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return SERVICE_INVALID_MONEY_FORMAT;
	}

	val = atoi(ptr);
	if (val < 0) {
		*resp = invalid_money_format;
		return SERVICE_INVALID_MONEY_FORMAT;
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
	n = (HORSE_NAME + 5) * HORSE_RUN;
	*resp = _malloc(n);
	memset(*resp, 0, n);
	ptr = *resp;
	for (i = 0; i < HORSE_RUN; ++i) {
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
		return SERVICE_RACE_FIRST_RUN;
	}
	strncpy(*resp, user->service->win->name, HORSE_NAME);
	(*resp)[strlen(*resp)] = '\n';	

	return EXIT_SUCCESS;
}

byte req_nextrun(const char *req, char **resp, struct user *user) {
	*resp = get_next_race_time(user->service);		
	return SERVICE_NON_FREE;
}

byte req_bet(const char *req, char **resp, struct user *user) {
	char *ptr, *_ptr;
	int val, i;
	if (!user->name[0]) {
		*resp = login_before;
		return SERVICE_LOGIN_NONE; 
	}
	ptr = req;
	while (*ptr && !isdigit(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return SERVICE_INVALID_BET_FORMAT;
	}

	val = atoi(ptr);
	if (val < 0 || (unsigned int)val > user->money) {
		*resp = invalid_money_format;
		return SERVICE_INVALID_BET_FORMAT;
	}

	// check horse
	while (*ptr && !isalpha(*ptr))
		++ptr;	

	if (!*ptr) {
		*resp = invalid_money_format;
		return SERVICE_INVALID_BET_FORMAT;
	}
	_ptr = ptr;
	while (*_ptr)
		++_ptr;
	
	user->bet = val;
	*resp = _malloc(64 * sizeof(char));

	// search for horse in currently running
	for (i = 0; i < HORSE_RUN; ++i) {
		if (!strcmp(ptr, user->service->current_run[i]->name)) {
			user->horse = user->service->current_run[i];
			++user->service->horse_bet[i];
			user->id = i;
			break;
		}
	}
	
	if (i == HORSE_RUN) {
		*resp = horse_absent;
		return SERVICE_RACE_ABSENT_HORSE;
	}

	// add money to bank accout
	_pthread_mutex_lock(user->service->mbank);
	user->service->bank += (unsigned int)val;
	user->money -= (unsigned int)val;
	_pthread_mutex_unlock(user->service->mbank);
	
	snprintf(*resp, 64, "Bet %u credits on %s horse.\n", (unsigned int)val, ptr);

	return EXIT_SUCCESS;
}

byte req_info(const char *req, char **resp, struct user *user) {
	*resp = info;	
	return SERVICE_NON_FREE;
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
	
	if (j == CLI_FUNC) {
inv_mth:
		*resp = invalid_method;
		return SERVICE_INVALID_METHOD;
	}
	
	byte (*fn)(const char *, char **, struct user *user);
	fn = requests[j];
	
	return fn(buf + REQ_MTHD, resp, user);
}

// ================================
// CLIENT THREAD & HELPER FUNCTIONS
// ================================

void init_client(struct user *dest, struct user *src) {
	memset(dest->name, 0, HORSE_NAME + 1);
	//dest->mutex = src->mutex;
	//dest->cond = src->cond;
	dest->mhb = src->mhb;
	dest->money = dest->bet = 0;
	dest->service = src->service;
	dest->id = -1;
	free(src->sockfd);
	free(src);
}

// TODO: send money at the end of race
void* client_thread(void *arg) {
	struct user *__user = (struct user *)arg; 
	struct user user;
	int sockfd = *(__user->sockfd), j, ret;
	size_t n;
	int i, nready;
	char buf[MSG_MAXLEN + 1];
	char horse_buf[(HORSE_NAME + 13) * (HORSE_RUN + 1)];
	char *resp, *p;
	unsigned int _money;
	byte send_win = 0;
	fd_set rset, active;
	struct timeval tv, active_tv = { 0, 0 };

	FD_ZERO(&active);
	FD_SET(sockfd, &active);

	init_client(&user, __user);
	_pthread_detach(pthread_self());	

	while (work) {
handle_conn:
		if (!run) {
			send_win = 0;
			rset = active;
			tv = active_tv;

			nready = select(sockfd + 1, &rset, NULL, NULL, &tv);
			if ((nready < 0 && errno == EINTR) || !nready)
				continue;
		
			// someone wants to write	
			// write on client terminal
			if (!(n = read(sockfd, buf, MSG_MAXLEN))) {
				if (errno == EINTR) {
					fprintf(stderr, "interrupted by signal\n");
					continue;
				}
				break;
			}
			else {
				buf[n < MSG_MAXLEN ? n : MSG_MAXLEN] = 0;
				if (buf[0] == '\r' || buf[0] == '\n')
					continue;
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
			if (send_win)
				continue;

			_pthread_mutex_lock(user.service->mfinished);	
			if (!user.service->finished) {
				p = horse_buf;	
				for (i = 0; i < HORSE_RUN; ++i) {
					n = snprintf(p, HORSE_NAME + 12, "%s: %u\n", user.service->current_run[i]->name, user.service->current_run[i]->distance);
					p += n;	
				}
				snprintf(p, HORSE_NAME + 12, "%s\n", "====================");					
					
				_write(sockfd, (void *)horse_buf, strlen(horse_buf));
	
			}
			else {
				_money = 0;

				if (user.horse && !strcmp(user.horse->name, user.service->win->name)) {
					_pthread_mutex_lock(user.service->mbank);

					_pthread_mutex_lock(user.service->mhb);	
					_money = user.service->bank / user.service->copy_horse_bet[user.id];
					--user.service->copy_horse_bet[user.id];	
					user.service->bank -= _money;
					user.money += _money;
					
					_pthread_mutex_unlock(user.service->mhb);
					_pthread_mutex_unlock(user.service->mbank);			
				} 
				
				snprintf(buf, MSG_MAXLEN * 2, "Winner: %s; you won %u; your current accout: %u\n", user.service->win->name, _money, user.money);
				_write(sockfd, (void *)buf, strlen(buf));
				
				send_win = 1;
				user.horse = NULL;
			}	
			_pthread_mutex_unlock(user.service->mfinished);	
			
			_sleep(1, 0);
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
	ph->strength = HORSE_START_STRENGTH;
	ph->distance = 0;
	ph->running = 0;
	ph->mutex = rm;
	ph->cond = rc;
	ph->barrier = rb;
	ph->service = service;
	_pthread_create(&(ph->tid), NULL, horse_thread, (void *)ph);
}

unsigned int horse_make_step(struct horse *horse) {
	double rnd = 0.5 + (double)rand() / (double)RAND_MAX;
	unsigned int ret = 5 + (((10 - rand() % 5) * (horse->strength / 10 + 1)) / 10);
	if (horse->strength < 10)
		return 5;

	horse->strength = (horse->strength - ret * rnd);
	if (horse->strength < 0)
		horse->strength = 7;


	fprintf(stderr, "step: %d\n", ret);
	return ret;
}

unsigned int horse_restore_strength(unsigned int strength) {
	double rnd = (double)rand() / (double)RAND_MAX;
	unsigned int ret = (500 * rnd) / strength + rand() % 5 + 1;
	fprintf(stderr, "restore %u\n", ret);
	return ret;
}

void* horse_thread(void *arg) {
	struct horse *horse = (struct horse *)arg;
	struct service *service = horse->service;

	_pthread_detach(pthread_self());	

start:
	_pthread_mutex_lock(horse->mutex);

	while(!horse->running) {
		// restore strenght
		if (horse->strength < HORSE_START_STRENGTH) {
			horse->strength = (horse->strength + horse_restore_strength(horse->strength));
			if (horse->strength >= HORSE_START_STRENGTH)
				horse->strength = HORSE_START_STRENGTH;
			fprintf(stderr, "restore horse strength: %d\n", horse->strength);	
		}
		_pthread_cond_wait(horse->cond, horse->mutex);
	}

	_pthread_mutex_unlock(horse->mutex);

	while(horse->running) {
		_pthread_mutex_lock(service->mfinished);
		if (service->finished) {
			_pthread_mutex_unlock(service->mfinished);
			break;
		}
		
		_pthread_mutex_unlock(service->mfinished);

		// make step	
		horse->distance += horse_make_step(horse);
		fprintf(stderr, "horse %s: %u distance\n", horse->name, horse->distance);	
		
		// means that horse has finished a track
		if (horse->distance >= TRACK_DISTANCE) {
			fprintf(stderr, "horse %s: got a distance\n", horse->name);
			_pthread_mutex_lock(service->mfinished);
			if (service->finished) {
				_pthread_mutex_unlock(service->mfinished);
				break;
			}
			fprintf(stderr, "horse %s: WINS\n", horse->name);	
			service->win = horse;
			service->finished = 1;

			_pthread_mutex_unlock(service->mfinished);
			break;
		}	

		_pthread_cond_wait(horse->cond, horse->mutex);
		_pthread_mutex_unlock(horse->mutex);
	}

	// continue waiting after finishing a run	
	fprintf(stderr, "horse %s had finished\n", horse->name);
	
	horse->running = 0;
	horse->distance = 0;

	_pthread_mutex_lock(service->mcur_run);
	--service->cur_run;
	fprintf(stderr, "cur_run %d\n", service->cur_run);
	_pthread_mutex_unlock(service->mcur_run);

	goto start;

	return NULL;
}

// ==============================
// SERVER WORK & HELPER FUNCTIONS
// ==============================

void choose_run_horses(struct horse *horses, size_t horse_number, struct service *service) {
	size_t cnt = 0;
	int i = 0;
	byte *rh = _malloc(sizeof(byte) * horse_number);

	memset(rh, 0, sizeof(byte) * horse_number);

	if (horse_number < HORSE_RUN) {
		fprintf(stderr, "Current number of horses is %d, should be at least %d\n", horse_number, HORSE_RUN);
		exit(EXIT_FAILURE);
	}
	// set all running	
	if (horse_number == HORSE_RUN) {
		for (i = 0; i < HORSE_RUN; ++i)
			service->current_run[i] = &horses[i];	
		return;
	}

	// choose randomly
	while(cnt != HORSE_RUN) {	
		i = rand() % horse_number;
		//fprintf(stderr, "random: %d\n", i);
		if (!rh[i]) {
			rh[i] = 1;
			//fprintf(stderr, "choose: %d %s\n", i, horses[i].name);
			//fflush(stderr);
			service->current_run[cnt++] = &horses[i];
		}
	}
}

void start_horses(struct service *service) {
	int i;
	for (i = 0; i < HORSE_RUN; ++i)
		service->current_run[i]->running = 1;
}

void init_race(struct service *service) {
	// copy horse bets
	_pthread_mutex_lock(service->mhb);
	memcpy(service->copy_horse_bet, service->horse_bet, sizeof(unsigned int) * HORSE_RUN);	
	_pthread_mutex_unlock(service->mhb);
	
	// be sure that noone is using value
	_pthread_mutex_lock(service->mfinished);
	service->finished = 0;
	_pthread_mutex_unlock(service->mfinished);

	start_horses(service);
}

void post_race(struct service *service, struct horse *horses, size_t horse_num) {
	_pthread_mutex_lock(service->mcur_run);
	service->cur_run = HORSE_RUN;
	_pthread_mutex_unlock(service->mcur_run);

	// choose new horses
	memset(service->current_run, 0, sizeof(struct horse *) * HORSE_RUN);
	choose_run_horses(horses, horse_num, service);	
	memset(service->horse_bet, 0, sizeof(unsigned int) * HORSE_RUN);
	
	// set new time for next run
	set_next_race_time(service);		

	run = 0; 

	alarm(service->delay);
}

void play(pthread_cond_t *cond, struct service *service, struct horse *horses, size_t horse_num) {
	int i;
	/*	
	// copy horse bets
	_pthread_mutex_lock(service->mhb);
	memcpy(service->copy_horse_bet, service->horse_bet, sizeof(unsigned int) * HORSE_RUN);	
	_pthread_mutex_unlock(service->mhb);
	
	// be sure that noone is using value
	_pthread_mutex_lock(service->mfinished);
	service->finished = 0;
	_pthread_mutex_unlock(service->mfinished);

	start_horses(service);
	*/
	init_race(service);

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
				//sleep(1);
				_sleep(1, 0);
				goto check_for_horses;
			}
		}
		_pthread_mutex_unlock(service->mfinished);	
		fprintf(stderr, "=========================\nmain: sleeping 1 sec\n===============================\n");
		// sleep(1);	
		_sleep(1, 0);
		_pthread_cond_broadcast(cond);
	}

	post_race(service, horses, horse_num);
	/*
	_pthread_mutex_lock(service->mcur_run);
	service->cur_run = HORSE_RUN;
	_pthread_mutex_unlock(service->mcur_run);

	// choose new horses
	memset(service->current_run, 0, sizeof(struct horse *) * HORSE_RUN);
	choose_run_horses(horses, horse_num, service);	
	memset(service->horse_bet, 0, sizeof(unsigned int) * HORSE_RUN);
	
	// set new time for next run
	set_next_race_time(service);		

	run = 0; 

	alarm(service->delay);
	*/
} 

void server_work(int listenfd, sigset_t *sint, pthread_cond_t *cond, pthread_mutex_t *mutex, struct service *service, struct horse *horses, size_t horse_num) {
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

	alarm(service->delay);

	while (work) {
		// accepting client, while there is no run
		if (!run) {
			clilen = sizeof(cliaddr); 
			iptr = _malloc(sizeof(int));
			*iptr = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);	
			if (run) {
				free(iptr);
				continue;
			}
			user = (struct user *)_malloc(sizeof(struct user));
			user->sockfd = iptr;
			user->service = service;
			user->mnr = service->mnr;
			user->mhb = service->mhb;
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
		return FILE_GETLINE_ERROR;
		
	tmp = atoi(line);	
	free(line);
	if (tmp <= 0)
		return FILE_INVALID_FORMAT;
	*pval = tmp;
	return EXIT_SUCCESS;
}

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
	else return FILE_CANONICALIZE_ERROR;		

	return EXIT_SUCCESS;
}

// =================
// SERVICE FUNCTIONS
// =================
void init_service(struct service *service, pthread_mutex_t *mf, pthread_mutex_t *mb, pthread_mutex_t *mcr, pthread_mutex_t *mhb, pthread_mutex_t *mnr, unsigned int delay) {
	service->mfinished = mf;
	service->mbank = mb;
	service->mhb = mhb;
	service->mnr = mnr;
	service->bank = 0;
	service->finished = 0;
	service->mcur_run = mcr;
	service->cur_run = HORSE_RUN;
	service->delay =  delay;
	set_next_race_time(service);
	service->win = NULL;
	memset(service->horse_bet, 0, sizeof(unsigned int) * HORSE_RUN);
	memset(service->current_run, 0, sizeof(struct horse *) * HORSE_RUN);
}

// ==============================================
// MAIN & USAGE FUNCTIONS & MAIN HELPER FUNCTIONS
// ==============================================

void init_sync(pthread_mutex_t *mutex, pthread_cond_t *cond, pthread_mutex_t *mfinished, pthread_mutex_t *mbank,
				pthread_mutex_t *mcur_run, pthread_mutex_t *mhb, pthread_mutex_t *mnr) {
	// init synchronization info
	_pthread_mutex_init(mutex, NULL);
	_pthread_cond_init(cond, NULL);

	// service info
	_pthread_mutex_init(mfinished, NULL);
	_pthread_mutex_init(mbank, NULL);
	_pthread_mutex_init(mcur_run, NULL);
	_pthread_mutex_init(mhb, NULL);
	_pthread_mutex_init(mnr, NULL);
}

int init_socket(int port) {
	int listenfd, t;
	struct sockaddr_in servaddr;

	listenfd = _socket(PF_INET, SOCK_STREAM, 0);

	memset(&servaddr, sizeof(char), sizeof(servaddr));			

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	_setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t));	

	_bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	_listen(listenfd, LISTENQ);
}

void clean(pthread_mutex_t *mutex, pthread_cond_t *cond, pthread_mutex_t *mfinished, pthread_mutex_t *mbank,
				pthread_mutex_t *mcur_run, pthread_mutex_t *mhb, pthread_mutex_t *mnr, struct horse *horses, int listenfd) {
	fprintf(stderr, "Start cleaning...\n");
	fflush(stderr);
	_close(listenfd);
	fprintf(stderr, "Global mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(mutex);
	fprintf(stderr, "Finished mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(mfinished);
	fprintf(stderr, "Bank mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(mbank);
	fprintf(stderr, "Current run mutex\n");
	fflush(stderr);
	_pthread_mutex_destroy(mcur_run);
	_pthread_mutex_destroy(mhb);
	_pthread_mutex_destroy(mnr);

	free(horses);	
}

void usage(const char *name) {
	fprintf(stderr, "usage: %s <port> <config file>\n", name);
}

// TODO: add init_socket
int main(int argc, char **argv) {
	int i, listenfd, port, ret, t;
	struct sockaddr_in servaddr;
	sigset_t sint, sempty;
	struct horse *horses;
	unsigned int horse_num, pperh;	
	pthread_mutex_t mutex, mfinished, mbank, mcur_run, mhb, mnr;
	pthread_cond_t cond;
	pthread_barrier_t barrier;		
	struct service service;

	if (argc != 3) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/*parse port*/
	port = atoi(argv[1]);

	//listenfd = init_socket(port);

	listenfd = _socket(PF_INET, SOCK_STREAM, 0);
	memset(&servaddr, sizeof(char), sizeof(servaddr));			
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	_setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t));	
	_bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	_listen(listenfd, LISTENQ);

	init_sync(&mutex, &cond, &mfinished, &mbank, &mcur_run, &mhb, &mnr);	

	ret = parse_conf_file(argv[2], &horses, &horse_num, &pperh, &mutex, &cond, &barrier, &service);

	if (ret)
		goto clean;	
	
	if (!pperh || pperh > 60) {
		fprintf(stderr, "races per hour %u should be more than 0 and less than 61\n", pperh);
		goto clean;
	}
#if 1
	fprintf(stderr, "delay = %u\n", (60 * 60) / pperh);
	fflush(stderr);
#endif
	init_signals();	
	srand(time(NULL));	
	init_service(&service, &mfinished, &mbank, &mcur_run, &mhb, &mnr, (unsigned int)(60 * 60 / pperh));	

	server_work(listenfd, &sint, &cond, &mutex, &service, horses, horse_num);
	
clean:
	/*closing listen file desc*/
	clean(&mutex, &cond, &mfinished, &mbank, &mcur_run, &mhb, &mnr, horses, listenfd);

	return EXIT_SUCCESS;
}
