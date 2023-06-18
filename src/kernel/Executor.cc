/*
  Copyright (c) 2019 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Xie Han (xiehan@sogou-inc.com)
*/

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "list.h"
#include "thrdpool.h"
#include "Executor.h"

struct ExecSessionEntry
{
	struct list_head list;
	ExecSession *session;
	thrdpool_t *thrdpool;
};

int ExecQueue::init()
{
	int ret;

	ret = pthread_mutex_init(&this->mutex, NULL);
	if (ret == 0)
	{
		INIT_LIST_HEAD(&this->session_list);
		return 0;
	}

	errno = ret;
	return -1;
}

void ExecQueue::deinit()
{
	pthread_mutex_destroy(&this->mutex);
}

int Executor::init(size_t nthreads)
{
	if (nthreads == 0)
	{
		errno = EINVAL;
		return -1;
	}

	this->thrdpool = thrdpool_create(nthreads, 0);
	if (this->thrdpool)
		return 0;

	return -1;
}

void Executor::deinit()
{
	thrdpool_destroy(Executor::executor_cancel, this->thrdpool);
}

extern "C" void __thrdpool_schedule(const struct thrdpool_task *, void *,
									thrdpool_t *);

void Executor::executor_thread_routine(void *context)
{
	// NOTE: This func is run in thrdpool ?
	ExecQueue *queue = (ExecQueue *)context;
	struct ExecSessionEntry *entry;
	ExecSession *session;

	pthread_mutex_lock(&queue->mutex);
	entry = list_entry(queue->session_list.next, struct ExecSessionEntry, list);
	list_del(&entry->list);
	// session points to the user Subtask object.
	session = entry->session;
	if (!list_empty(&queue->session_list))
	{
		// thrdpool_task is designed to handle not only WFThreadTask and also other tasks. 
		// So it need keep the specific task queue and associated routine.
		struct thrdpool_task task = {
			.routine	=	Executor::executor_thread_routine,
			.context	=	queue
		};
		// If the queue still has tasks other than the current entry, continue scheduling.
		// NOTE: 
		// 1) how to make sure the tasks in the same series are executed in sequential order ?
		//   Seems tasks in the same ExecQueue are not required to be executed sequentially.
		// 2) here the second argument, for param buf, is not type of __thrdpool_task_entry*.
		// This is ok here because entry is ready to free since it's removed from the ExecQueue.
		// The idea is to reuse the buf and aovid unnecessary memory dealloc and alloc. It's ok as long as the buf
		//   is at least larger than __thrdpool_task_entry.
		__thrdpool_schedule(&task, entry, entry->thrdpool);
	}
	else
		free(entry);

	pthread_mutex_unlock(&queue->mutex);
	session->execute();
	// This handle() will call the callback of the task, and  will schedule the next task in the same series.
	session->handle(ES_STATE_FINISHED, 0);
}

void Executor::executor_cancel(const struct thrdpool_task *task)
{
	ExecQueue *queue = (ExecQueue *)task->context;
	struct ExecSessionEntry *entry;
	struct list_head *pos, *tmp;
	ExecSession *session;

	list_for_each_safe(pos, tmp, &queue->session_list)
	{
		entry = list_entry(pos, struct ExecSessionEntry, list);
		list_del(pos);
		session = entry->session;
		free(entry);

		session->handle(ES_STATE_CANCELED, 0);
	}
}

int Executor::request(ExecSession *session, ExecQueue *queue)
{
	struct ExecSessionEntry *entry;

	session->queue = queue;
	entry = (struct ExecSessionEntry *)malloc(sizeof (struct ExecSessionEntry));
	if (entry)
	{
		// session points to the user Subtask object.
		entry->session = session;
		entry->thrdpool = this->thrdpool;
		pthread_mutex_lock(&queue->mutex);
		// Put the task in the ExecQueue waiting being scheduled for running in thrdpool.
		// After list_add_tail(), queue becomes ...<-->entry.list<-->queue.session_list<-->...
		list_add_tail(&entry->list, &queue->session_list);
		if (queue->session_list.next == &entry->list)
		{
			// This if-cond checks if entry.list is the first node in the list.
			// If it's the only single task in the queue, put it in the queue of thrdpool directly, i.e., schedule it for running.
			struct thrdpool_task task = {
				.routine	=	Executor::executor_thread_routine,
				.context	=	queue
			};
			if (thrdpool_schedule(&task, this->thrdpool) < 0)
			{
				list_del(&entry->list);
				free(entry);
				entry = NULL;
			}
		}
		// NOTE: if the queue has more than 1 tasks, when will they be scheduled as thrdpool_schedule() ?

		pthread_mutex_unlock(&queue->mutex);
	}

	return -!entry;
}

