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

#include "SubTask.h"

void SubTask::subtask_done()
{
	SubTask *cur = this;
	ParallelTask *parent;

	while (1)
	{
		parent = cur->parent;
		// done() usually calls callback. But it's not always the case, becas some SubTask has no callback, e.g., Processor in WFServerTask.
		// done() always pops and returns next task in the series.
		// The returned task may be one added in the series by the callback of the preivous tasks.
		cur = cur->done();
		// cur now is the next task after current one in the series.
		if (cur)
		{
			// NOTE: why need set parent even it's from the same series ?
			cur->parent = parent;
			// This task is poped from the series after the previous one is finished. So this task and its callback will be finished after,
			//   no matter which thread will run them. This ensure tasks are executed in sequential order.
			cur->dispatch();
		}
		else if (parent)
		{
			if (__sync_sub_and_fetch(&parent->nleft, 1) == 0)
			{
				cur = parent;
				continue;
			}
		}

		break;
	}
}

void ParallelTask::dispatch()
{
	SubTask **end = this->subtasks + this->subtasks_nr;
	SubTask **p = this->subtasks;

	this->nleft = this->subtasks_nr;
	if (this->nleft != 0)
	{
		do
		{
			(*p)->parent = this;
			(*p)->dispatch();
		} while (++p != end);
	}
	else
		this->subtask_done();
}

