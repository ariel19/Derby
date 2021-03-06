#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <pthread.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include "lib.h"

ssize_t _write(int sockfd, void *vptr, size_t n) {
	char *buf = vptr;
	int nleft = n,
		nwritten = 0;
	while(nleft > 0) {
		if((nwritten = write(sockfd, buf, nleft)) <= 0) {
			if(errno == EINTR)
				nwritten = 0;
			else if (errno == EPIPE)
				return 0;
			else return -1;
		}
		nleft -= nwritten;
		buf += nwritten;
	}
	return n;
}

int _socket(int domain, int type, int protocol) {
	int fd;
	if ((fd = socket(domain, type, protocol)) < 0)
		ERR("socket");
	return fd;	
}

void _setsockopt(int sockfd, int level, int optname, void *optval, socklen_t optlen) {
	if (setsockopt(sockfd, level, optname, optval, optlen))
		ERR("setsockopt");
}

void _bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	if (bind(sockfd, addr, addrlen))
		ERR("bind");
}

void _listen(int sockfd, int backlog) {
	if (listen(sockfd, backlog))
		ERR("listen");
}

int _accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	int fd;
again:
	if ((fd = accept(sockfd, addr, addrlen)) < 0) {
		if (errno == EINTR)
			goto again;
		else ERR("accept");
	}
	return fd;
}

void _close(int sockfd) {
	if (close(sockfd))
		ERR("close");
}

void (*_signal(int signo, void (*func)(int)))(int) {
	struct sigaction act, old;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if(sigaction(signo, &act, &old) < 0)
		return SIG_ERR;
	return old.sa_handler;
}

void _sigemptyset(sigset_t *set) {
	if (sigemptyset(set) < 0)
		ERR("sigemptyset");
}

void _sigaddset(sigset_t *set, int sig) {
	if (sigaddset(set, sig) < 0)
		ERR("sigaddset");
}

void _sigprocmask(int how, const sigset_t *set, sigset_t *old) {
		if (sigprocmask(how, set, old) < 0)
			ERR("sigprocmask");
}

void * _malloc(int size) {
	void *buf;
	if (!(buf = malloc(size)))
		ERR("malloc");
	return buf;
}

void _sleep(time_t s, long ns) {
	struct timespec tt, t = { s, ns };	
	for (tt = t; nanosleep(&tt, &tt);)
		if (errno != EINTR)
			ERR("nanosleep");
}

void _pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
	if (pthread_create(thread, attr, start_routine, arg) != 0)
		ERR("pthread_create");
}

void _pthread_detach(pthread_t thread) {
	if (pthread_detach(thread))
		ERR("pthread_detach");
}

void _pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *mutexattr) {
	if (pthread_mutex_init(mutex, mutexattr))
		ERR("pthread_mutex_init");
}

void _pthread_mutex_destroy(pthread_mutex_t *mutex) {
	if (pthread_mutex_destroy(mutex))
		ERR("pthread_mutex_destroy");
}

void _pthread_mutex_lock(pthread_mutex_t *mutex) {
	if (pthread_mutex_lock(mutex))
		ERR("pthread_mutex_lock");
}

void _pthread_mutex_unlock(pthread_mutex_t *mutex) {
	if (pthread_mutex_unlock(mutex))
		ERR("pthread_mutex_unlock");
}

void _pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cond_attr) {
	if (pthread_cond_init(cond, cond_attr))
		ERR("pthread_cond_init");
}

void _pthread_cond_signal(pthread_cond_t *cond) {
	if (pthread_cond_signal(cond))
		ERR("pthread_cond_signal");
}

void _pthread_cond_broadcast(pthread_cond_t *cond) {
	if (pthread_cond_broadcast(cond))
		ERR("pthread_cond_broadcast");
}

void _pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
	if (pthread_cond_wait(cond, mutex))
		ERR("pthread_cond_wait");
}

void _pthread_cond_destroy(pthread_cond_t *cond) {
	if (pthread_cond_destroy(cond))
		ERR("pthread_cond_destroy");
}

void _pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned count) {
	if (pthread_barrier_init(barrier, attr, count))	
		ERR("pthread_barrier_init");
}

void _pthread_barrier_wait(pthread_barrier_t *barrier) {
	if (pthread_barrier_wait(barrier))
		ERR("pthread_barrier_wait");
}

void _pthread_barrier_destroy(pthread_barrier_t *barrier) {
	if (pthread_barrier_destroy(barrier))
		ERR("pthread_barrier_destroy");
}

void _pthread_sigmask(int how, const sigset_t *set, sigset_t *oldest) {
	if(pthread_sigmask(how, set, oldest))
		ERR("pthread_sigmask");
}
