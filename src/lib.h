#pragma once

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source), exit(EXIT_FAILURE))

extern ssize_t _write(int sockfd, void *vptr, size_t n);
extern int _socket(int domain, int type, int protocol);
extern void _bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern void _listen(int sockfd, int backlog);
extern int _accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern void _close(int sockfd);
extern void (*_signal(int signo, void (*func)(int)))(int);
extern void _sigemptyset(sigset_t *set);
extern void _sigaddset(sigset_t *set, int sig);
extern void _sigprocmask(int how, const sigset_t *set, sigset_t *old);
extern void * _malloc(int size);

// pthreads
extern void _pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg); 
extern void _pthread_detach(pthread_t thread);
// mutexes
extern void _pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *mutexattr);
extern void _pthread_mutex_destroy(pthread_mutex_t *mutex);
extern void _pthread_mutex_lock(pthread_mutex_t *mutex);
extern void _pthread_mutex_unlock(pthread_mutex_t *mutex);
// conditionals
extern void _pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cond_attr);
extern void _pthread_cond_signal(pthread_cond_t *cond);
extern void _pthread_cond_broadcast(pthread_cond_t *cond);
extern void _pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
extern void _pthread_cond_destroy(pthread_cond_t *cond);
// barriers
extern void _pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned count);
extern void _pthread_barrier_wait(pthread_barrier_t *barrier);
extern void _pthread_barrier_destroy(pthread_barrier_t *barrier);
// signals
extern void _pthread_sigmask(int how, const sigset_t *set, sigset_t *oldest);
