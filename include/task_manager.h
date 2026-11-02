
#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "worker.h"

#define MAX_TASKS 10

typedef struct {
    WorkerTask tasks[MAX_TASKS];
    int task_count;
} TaskManager;

void task_manager_init(TaskManager *manager);

int task_manager_add(
    TaskManager *manager,
    WorkerTask task
);

void task_manager_update_status(
    TaskManager *manager,
    int task_id,
    TaskStatus status
);

void task_manager_increment_retry(
    TaskManager *manager,
    int task_id
);

void task_manager_display(
    const TaskManager *manager
);

#endif
