
#include <sys/types.h>
#include <sys/socket.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>

#define MAXTID 1000	/* max number of threads */

#define SERV_PORT 4321

struct job
{
	int job_connfd;
};

struct job_queue
{
	struct job_queue *jq_next;
	struct job *jq_job;
};

struct job_queue *jq_head;
struct job_queue *jq_tail;

pthread_mutex_t jqlock;
pthread_cond_t jqcv;

void *
worker(void *arg)
{
	struct job_queue *jq;

	for (;;) {
		pthread_mutex_lock(&jqlock);
		while (!jq_head)
			pthread_cond_wait(&jqcv, &jqlock);

		while ((jq = jq_head) != NULL) {
			close(jq->jq_job->job_connfd);
			free(jq->jq_job);
			jq_head = jq->jq_next;
			free(jq);
		}
		pthread_mutex_unlock(&jqlock);
	}
}

int
main(int argc, char *argv[])
{
	pthread_t tid[MAXTID];
	int tids = MAXTID;
	int i, rval;
	int listenfd, connfd;
	struct sockaddr_in servaddr;
	struct sockaddr cliaddr;
	struct job *newjob;
	socklen_t len = sizeof(struct sockaddr);
	struct job_queue *jq;
	pthread_attr_t attr;
	int one = 1;

	if (argc == 2) {
		tids = atoi(argv[1]);
		if (tids <= 0 || tids > MAXTID-1) {
			fprintf(stderr, "Usage: %s [nthreads]\n", argv[0]);
			exit(1);
		}
	}

	pthread_mutex_init(&jqlock, NULL);
	pthread_cond_init(&jqcv, NULL);
	
	pthread_attr_init(&attr);

	if (pthread_attr_setstacksize(&attr, 8192) != 0) {
		fprintf(stderr, "pthread_attr_setstacksize failed\n");
		exit(1);
	}

	for (i = 0; i < tids; i++) {
		rval = pthread_create(&tid[i], &attr, worker, NULL);
		if (rval != 0) {
			fprintf(stderr, "pthread_create failed, errno = %d\n", rval);
			exit(1);
		}
	}

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0) {
		fprintf(stderr, "socket call failed!\n");
		exit(1);
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one)) < 0) {
		fprintf(stderr, "setsockopt failed, errno = %d\n", errno);
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);

	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "bind failed\n");
		exit(1);
	}

	if (listen(listenfd, 5) < 0) {
		fprintf(stderr, "listen failed\n");
		exit(1);
	}

	for (;;) {
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
		if (connfd < 0) {
			fprintf(stderr, "accept failed\n");
			exit(1);
		}

		if (setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one)) < 0) {
			fprintf(stderr, "setsockopt failed, errno = %d\n", errno);
			exit(1);
		}
		
		if ((newjob = malloc(sizeof(struct job))) == NULL) {
			fprintf(stderr, "malloc failed\n");
			exit(1);
		}

		newjob->job_connfd = connfd;
		pthread_mutex_lock(&jqlock);
		if (!jq_head) {
			if ((jq_head = malloc(sizeof(struct job_queue))) == NULL) {
				fprintf(stderr, "failed to allocate job_queue\n");
				exit(1);
			}
			jq_tail = jq_head;
			jq_head->jq_next = jq_tail->jq_next = NULL;
			jq_head->jq_job = jq_tail->jq_job = newjob;
		} else {
			if ((jq = malloc(sizeof(struct job_queue))) == NULL) {
				fprintf(stderr, "failed to allocate job_queue\n");
				exit(1);
			}
			jq_tail->jq_next = jq;
			jq_tail = jq;
			jq_tail->jq_job = newjob;
			jq->jq_next = NULL;
		}
		pthread_cond_signal(&jqcv);
		pthread_mutex_unlock(&jqlock);
	}

	
	for (i = 0; i < tids; i++)
		pthread_join(tid[i], NULL);

	exit(0);
}

		
