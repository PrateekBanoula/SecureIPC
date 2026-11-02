
#include <stdio.h>
#include "task_manager.h"

void task_manager_init(TaskManager *manager) {
    manager->task_count = 0;
}

int task_manager_add(TaskManager *manager, WorkerTask task) {
    if (manager->task_count >= MAX_TASKS)
        return -1;

    manager->tasks[manager->task_count] = task;
    manager->task_count++;

    return 0;
}

void task_manager_update_status(
    TaskManager *manager,
    int task_id,
    TaskStatus status
) {
    for (int i = 0; i < manager->task_count; i++) {
        if (manager->tasks[i].task_id == task_id) {
            manager->tasks[i].status = status;
            return;
        }
    }
}

void task_manager_increment_retry(
    TaskManager *manager,
    int task_id
) {
    for (int i = 0; i < manager->task_count; i++) {
        if (manager->tasks[i].task_id == task_id) {
            manager->tasks[i].retry_count++;
            return;
        }
    }
}

void task_manager_display(const TaskManager *manager) {
    printf("\n========== TASK STATUS ==========\n");

    for (int i = 0; i < manager->task_count; i++) {
        const WorkerTask *task = &manager->tasks[i];
        const char *status;

        switch (task->status) {
            case PENDING:   status = "PENDING"; break;
            case RUNNING:   status = "RUNNING"; break;
            case COMPLETED: status = "COMPLETED"; break;
            case FAILED:    status = "FAILED"; break;
            case RETRYING:  status = "RETRYING"; break;
            default:        status = "UNKNOWN"; break;
        }

        printf("Task %d | Records %zu-%zu | Status: %s | Retries: %d\n",
               task->task_id,
               task->start_record,
               task->end_record,
               status,
               task->retry_count);
    }

    printf("=================================\n");
}
