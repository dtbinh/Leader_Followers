#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "threadpool.h"

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  threadpool_init
 *  Description:  线程池初始化函数，thread_num:线程数，queue_max_num:队列任务最大数量。
 * =====================================================================================
 */
struct threadpool* threadpool_init(int thread_num, int queue_max_num)
{
	int i;
	struct threadpool *pool = NULL;

	pool = malloc(sizeof(struct threadpool));
	if(NULL == pool)
	{
		printf("failed to malloc threadpool!\n");
		return NULL;
	}
	pool->thread_num = thread_num;
	pool->queue_max_num = queue_max_num;
	pool->queue_cur_num = 0;
	pool->head = NULL;
	pool->tail = NULL;
	if(pthread_mutex_init(&(pool->mutex), NULL))
	{
		printf("failed to init mutex");
		return NULL;
	}
	if (pthread_cond_init(&(pool->queue_empty), NULL))
	{
		printf("failed to init queue_empty!\n");
		return NULL;
	}
	if (pthread_cond_init(&(pool->queue_not_empty), NULL))
	{
		printf("failed to init queue_not_empty!\n");
		return NULL;
	}
	if (pthread_cond_init(&(pool->queue_not_full), NULL))
	{
		printf("failed to init queue_not_full!\n");
		return NULL;
	}
	pool->pthreads = malloc(sizeof(pthread_t) * thread_num);
	if(NULL == pool->pthreads)
	{
		printf("failed to malloc pthreads!\n");
		return NULL;
	}
	pool->queue_close = 0;
	pool->pool_close = 0;
	for(i = 0; i < pool->thread_num; i++)
	{
		pthread_create(&(pool->pthreads[i]), NULL, threadpool_function, (void *)pool);
	}

	return pool;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  threadpool_add_job
 *  Description:  向线程池添加任务，任务添加到队列中。
 * =====================================================================================
 */
int threadpool_add_job(struct threadpool *pool, void* (*callback_function)(void *arg), void *arg)
{
	struct job *pjob;
	assert(pool != NULL);
	assert(callback_function != NULL);
	assert(arg != NULL);

	pthread_mutex_lock(&(pool->mutex));
	
	while((pool->queue_cur_num == pool->queue_max_num) && !(pool->queue_close || pool->pool_close))
	{
		pthread_cond_wait(&(pool->queue_not_full), &(pool->mutex));
	}

	if(pool->queue_close || pool->pool_close)
	{
		pthread_mutex_unlock(&(pool->mutex));
		return -1;
	}

	pjob = (struct job *) malloc(sizeof(struct job));
	if(NULL == pjob)
	{
		pthread_mutex_unlock(&(pool->mutex));
		return -1;
	}
	pjob->callback_function = callback_function;
	pjob->arg = arg;
	pjob->next = NULL;
	if(pool->head == NULL)
	{
		pool->head = pool->tail = pjob;
		pthread_cond_broadcast(&(pool->queue_not_empty));
	}
	else
	{
		pool->tail->next = pjob;
		pool->tail = pjob;
	}
	pool->queue_cur_num++;
	pthread_mutex_unlock(&(pool->mutex));
	return 0;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  threadpool_function
 *  Description:  线程执行函数，当队列有任务时获取任务并执行callback_function。
 * =====================================================================================
 */
void* threadpool_function(void *arg)
{
	struct threadpool *pool = (struct threadpool*)arg;
	struct job *pjob = NULL;
	while(1)
	{
		pthread_mutex_lock(&(pool->mutex));
		while((pool->queue_cur_num == 0) && !pool->pool_close)
		{
			pthread_cond_wait(&(pool->queue_not_empty), &(pool->mutex));
		}
		if(pool->pool_close)
		{
			pthread_mutex_unlock(&(pool->mutex));
			pthread_exit(NULL);
		}
		pool->queue_cur_num--;
		pjob = pool->head;
		if(pool->queue_cur_num == 0)
		{
			pool->head = pool->tail = NULL;
		}
		else
		{
			pool->head = pjob->next;
		}
		if(pool->queue_cur_num == 0)
		{
			pthread_cond_signal(&(pool->queue_empty));
		}
		if(pool->queue_cur_num == pool->queue_max_num - 1)
		{
			pthread_cond_broadcast(&(pool->queue_not_full));
		}
		pthread_mutex_unlock(&(pool->mutex));

		(*(pjob->callback_function))(pjob->arg);
		free(pjob);
		pjob = NULL;
	}
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  threadpool_destroy
 *  Description:  等待队列中任务完成后销毁线程池
 * =====================================================================================
 */
int threadpool_destroy(struct threadpool *pool)
{
	int i;
	struct job *p;
	assert(pool != NULL);
	pthread_mutex_lock(&(pool->mutex));
	if(pool->queue_close || pool->pool_close)
	{
		pthread_mutex_unlock(&(pool->mutex));
		return -1;
	}

	pool->queue_close = 1;
	while(pool->queue_cur_num != 0)
	{
		pthread_cond_wait(&(pool->queue_empty), &(pool->mutex));
	}

	pool->pool_close = 1;
	while(pool->queue_cur_num != 0)
	{
		pthread_cond_wait(&(pool->queue_empty), &(pool->mutex));
	}

	pool->pool_close = 1;
	pthread_mutex_unlock(&(pool->mutex));
	pthread_cond_broadcast(&(pool->queue_not_empty));
	pthread_cond_broadcast(&(pool->queue_not_full));
	for(i = 0; i < pool->thread_num; i++)
	{
		pthread_join(pool->pthreads[i], NULL);
	}

	pthread_mutex_destroy(&(pool->mutex));
	pthread_cond_destroy(&(pool->queue_empty));
	pthread_cond_destroy(&(pool->queue_not_empty));
	pthread_cond_destroy(&(pool->queue_not_full)); 
	free(pool->pthreads);
	while(pool->head != NULL)
	{
		p = pool->head;
		pool->head = p->next;
		free(p);
	}
	free(pool);
	return 0;
}
