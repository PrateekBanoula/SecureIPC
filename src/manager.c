
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#include "worker.h"
#include "task_manager.h"
#include "shared_memory.h"
#include "ipc_socket.h"

#define WORKER_COUNT 3
#define DATASET_FILE "data/dataset.csv"
#define MAX_RETRIES 1

typedef struct {
    pid_t pid;
    int task_pipe[2];
    int started;
    int wait_completed;
    int wait_status;
    WorkerTask task;
} WorkerProcess;


/* Initialize worker information. */
void init_worker(WorkerProcess *worker) {
    worker->pid = -1;
    worker->task_pipe[0] = -1;
    worker->task_pipe[1] = -1;
    worker->started = 0;
    worker->wait_completed = 0;
    worker->wait_status = 0;
}


/* Write complete data to a pipe. */
int write_all(int fd, const void *buffer, size_t size) {
    const char *ptr = buffer;
    size_t total = 0;

    while (total < size) {
        ssize_t n = write(fd, ptr + total, size - total);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        total += (size_t)n;
    }

    return 0;
}


/* Count CSV records using dynamically allocated line storage. */
int count_dataset_records(
    const char *filename,
    size_t *count
) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("Dataset open failed");
        return -1;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    int header_found = 0;

    *count = 0;

    while ((length = getline(&line, &capacity, file)) != -1) {
        if (strspn(line, " \t\r\n") == (size_t)length)
            continue;

        if (!header_found) {
            header_found = 1;
            continue;
        }

        (*count)++;
    }

    free(line);
    fclose(file);

    return header_found ? 0 : -1;
}


/* Close a worker's pipe descriptors. */
void close_worker(WorkerProcess *worker) {
    if (worker->task_pipe[0] != -1)
        close(worker->task_pipe[0]);

    if (worker->task_pipe[1] != -1)
        close(worker->task_pipe[1]);

    worker->task_pipe[0] = -1;
    worker->task_pipe[1] = -1;
}


/* Create a worker process. */
int spawn_worker(
    WorkerProcess workers[],
    int worker_count,
    int index,
    WorkerTask task,
    SharedData *shared
) {
    WorkerProcess *worker = &workers[index];

    worker->task = task;

    if (pipe(worker->task_pipe) == -1) {
        perror("Task pipe creation failed");
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        close_worker(worker);
        return -1;
    }

    if (pid == 0) {
        /* Child closes unrelated pipe descriptors. */
        for (int j = 0; j < worker_count; j++) {
            if (j == index) {
                close(workers[j].task_pipe[1]);
            } else {
                close_worker(&workers[j]);
            }
        }

        WorkerTask received_task;

        if (read(
                worker->task_pipe[0],
                &received_task,
                sizeof(received_task)
            ) != sizeof(received_task)) {
            close(worker->task_pipe[0]);
            _exit(2);
        }

        close(worker->task_pipe[0]);

        /* Controlled crash simulation for testing recovery. */
        const char *crash_task = getenv("SECUREIPC_CRASH_TASK");

        if (crash_task &&
            atoi(crash_task) == received_task.task_id &&
            received_task.worker_id <= WORKER_COUNT) {

            printf("[Worker %d] Simulating crash!\n",
                   received_task.worker_id);
            fflush(stdout);

            raise(SIGKILL);
        }

        DatasetStats stats;

        printf("[Worker %d] Started. PID: %d\n",
               received_task.worker_id, getpid());
        fflush(stdout);

        int result = worker_execute(
            DATASET_FILE,
            received_task,
            &stats
        );

        /* Store the result in shared memory. */
        if (shared_memory_store(
                shared,
                received_task.task_id,
                received_task.worker_id,
                result == 0,
                result == 0 ? &stats : NULL
            ) != 0) {
            _exit(3);
        }

        printf("[Worker %d] Stored result for Task %d.\n",
               received_task.worker_id,
               received_task.task_id);
        fflush(stdout);

        _exit(result == 0 ? 0 : 1);
    }

    /* Parent process. */
    worker->pid = pid;
    worker->started = 1;

    close(worker->task_pipe[0]);
    worker->task_pipe[0] = -1;

    return 0;
}


/* Send a task to a worker. */
int send_task(WorkerProcess *worker) {
    int result = write_all(
        worker->task_pipe[1],
        &worker->task,
        sizeof(worker->task)
    );

    close(worker->task_pipe[1]);
    worker->task_pipe[1] = -1;

    return result;
}


/* Wait for a worker and collect its termination status. */
int wait_for_worker(WorkerProcess *worker) {
    if (!worker->started)
        return -1;

    pid_t result;

    do {
        result = waitpid(
            worker->pid,
            &worker->wait_status,
            0
        );
    } while (result == -1 && errno == EINTR);

    if (result != worker->pid)
        return -1;

    worker->wait_completed = 1;
    return 0;
}


/* Verify process termination and shared-memory result. */
int worker_succeeded(
    const WorkerProcess *worker,
    const SharedResult *result
) {
    return worker->started &&
           worker->wait_completed &&
           WIFEXITED(worker->wait_status) &&
           WEXITSTATUS(worker->wait_status) == 0 &&
           result->success &&
           result->task_id == worker->task.task_id &&
           result->worker_id == worker->task.worker_id;
}


/* Execute a failed task using a replacement worker. */
int retry_task(
    WorkerTask *task,
    int new_worker_id,
    SharedData *shared,
    DatasetStats *final_result
) {
    WorkerProcess replacement;
    init_worker(&replacement);

    task->worker_id = new_worker_id;
    task->status = RETRYING;

    printf("\n[Manager] Reassigning Task %d to replacement Worker %d\n",
           task->task_id, new_worker_id);

    if (spawn_worker(
            &replacement,
            1,
            0,
            *task,
            shared
        ) != 0) {
        close_worker(&replacement);
        return -1;
    }

    if (send_task(&replacement) != 0)
        perror("Replacement task transmission failed");

    wait_for_worker(&replacement);

    SharedResult response = {0};

    if (shared_memory_read(
            shared,
            task->task_id,
            &response
        ) != 0) {
        close_worker(&replacement);
        return -1;
    }

    int success = worker_succeeded(&replacement, &response);

    if (success) {
        *final_result = response.stats;

        printf("[Manager] Replacement Worker %d recovered Task %d.\n",
               new_worker_id, task->task_id);
    } else {
        printf("[Manager] Replacement Worker %d failed Task %d.\n",
               new_worker_id, task->task_id);
    }

    close_worker(&replacement);

    return success ? 0 : -1;
}


static int run_dataset_analysis(void) {
    signal(SIGPIPE, SIG_IGN);

    printf("\n========== SECURE IPC MANAGER ==========\n");

    /* 1. Count dataset records. */
    size_t total_records;

    if (count_dataset_records(
            DATASET_FILE,
            &total_records
        ) != 0 || total_records == 0) {
        printf("[Manager] Dataset is empty or invalid.\n");
        return 1;
    }

    printf("[Manager] Dataset records: %zu\n", total_records);

    /* 2. Determine active workers. */
    int active_workers = total_records < WORKER_COUNT
                       ? (int)total_records
                       : WORKER_COUNT;

    printf("[Manager] Active workers: %d\n", active_workers);

    /* 3. Create shared memory and semaphore. */
    char shm_name[64];
    int shm_fd = -1;

    SharedData *shared = shared_memory_create(
        shm_name,
        sizeof(shm_name),
        &shm_fd
    );

    if (!shared)
        return 1;

    /* 4. Distribute dataset records dynamically. */
    WorkerTask tasks[WORKER_COUNT];

    size_t base = total_records / (size_t)active_workers;
    size_t remainder = total_records % (size_t)active_workers;
    size_t start = 1;

    TaskManager task_manager;
    task_manager_init(&task_manager);

    for (int i = 0; i < active_workers; i++) {
        size_t assigned = base + (i < (int)remainder ? 1 : 0);

        tasks[i] = (WorkerTask) {
            .task_id = i + 1,
            .worker_id = i + 1,
            .start_record = start,
            .end_record = start + assigned - 1,
            .status = PENDING,
            .retry_count = 0
        };

        start += assigned;

        if (task_manager_add(&task_manager, tasks[i]) != 0) {
            printf("[Manager] Task registration failed.\n");
            shared_memory_destroy(shared, shm_fd, shm_name);
            return 1;
        }

        printf("[Manager] Task %d: Records %zu-%zu\n",
               tasks[i].task_id,
               tasks[i].start_record,
               tasks[i].end_record);
    }

    /* 5. Initialize workers. */
    WorkerProcess workers[WORKER_COUNT];

    for (int i = 0; i < WORKER_COUNT; i++)
        init_worker(&workers[i]);

    int next_worker_id = active_workers + 1;

    /* 6. Create initial workers. */
    printf("\n[Manager] Creating workers...\n");

    for (int i = 0; i < active_workers; i++) {
        if (spawn_worker(
                workers,
                active_workers,
                i,
                tasks[i],
                shared
            ) != 0) {
            printf("[Manager] Could not create Worker %d.\n", i + 1);
            continue;
        }

        task_manager_update_status(
            &task_manager, tasks[i].task_id, RUNNING
        );

        printf("[Manager] Worker %d created. PID: %d\n",
               i + 1, workers[i].pid);
    }

    /* 7. Distribute tasks through pipes. */
    printf("\n[Manager] Distributing tasks through pipes...\n");

    for (int i = 0; i < active_workers; i++) {
        if (workers[i].started &&
            send_task(&workers[i]) != 0) {
            perror("Task transmission failed");
        }
    }

    DatasetStats final_results[WORKER_COUNT];
    int task_completed[WORKER_COUNT] = {0};

    /* 8. Wait for workers and read shared-memory results. */
    printf("\n[Manager] Waiting for workers...\n");

    for (int i = 0; i < active_workers; i++) {
        if (workers[i].started)
            wait_for_worker(&workers[i]);

        SharedResult response = {0};

        if (shared_memory_read(
                shared,
                tasks[i].task_id,
                &response
            ) != 0) {
            printf("[Manager] Could not read Task %d result.\n",
                   tasks[i].task_id);
        }

        if (worker_succeeded(&workers[i], &response)) {
            final_results[i] = response.stats;
            task_completed[i] = 1;

            task_manager_update_status(
                &task_manager,
                tasks[i].task_id,
                COMPLETED
            );

            printf("[Manager] Task %d completed by Worker %d.\n",
                   tasks[i].task_id,
                   tasks[i].worker_id);
        } else {
            if (workers[i].wait_completed &&
                WIFSIGNALED(workers[i].wait_status)) {
                printf("[Manager] Worker %d crashed with signal %d.\n",
                       tasks[i].worker_id,
                       WTERMSIG(workers[i].wait_status));
            } else {
                printf("[Manager] Task %d failed.\n",
                       tasks[i].task_id);
            }
        }

        close_worker(&workers[i]);
    }

    /* 9. Retry failed tasks. */
    printf("\n[Manager] Checking failed tasks...\n");

    for (int i = 0; i < active_workers; i++) {
        if (task_completed[i])
            continue;

        if (tasks[i].retry_count >= MAX_RETRIES) {
            task_manager_update_status(
                &task_manager, tasks[i].task_id, FAILED
            );
            continue;
        }

        tasks[i].retry_count++;

        task_manager_increment_retry(
            &task_manager, tasks[i].task_id
        );

        task_manager_update_status(
            &task_manager, tasks[i].task_id, RETRYING
        );

        int retry_result = retry_task(
            &tasks[i],
            next_worker_id++,
            shared,
            &final_results[i]
        );

        if (retry_result == 0) {
            task_completed[i] = 1;

            task_manager_update_status(
                &task_manager, tasks[i].task_id, COMPLETED
            );
        } else {
            task_manager_update_status(
                &task_manager, tasks[i].task_id, FAILED
            );
        }
    }

    /* 10. Aggregate valid results. */
    long total_count = 0;
    long long total_sum = 0;

    int minimum = 0;
    int maximum = 0;

    int has_result = 0;
    int completed_count = 0;

    printf("\n========== WORKER RESULTS ==========\n");

    for (int i = 0; i < active_workers; i++) {
        if (!task_completed[i])
            continue;

        DatasetStats *stats = &final_results[i];

        printf("Task %d: Count=%ld, Sum=%lld, Min=%d, Max=%d\n",
               tasks[i].task_id,
               stats->count,
               stats->sum,
               stats->minimum,
               stats->maximum);

        total_count += stats->count;
        total_sum += stats->sum;
        completed_count++;

        if (!has_result) {
            minimum = stats->minimum;
            maximum = stats->maximum;
            has_result = 1;
        } else {
            if (stats->minimum < minimum)
                minimum = stats->minimum;

            if (stats->maximum > maximum)
                maximum = stats->maximum;
        }
    }

    task_manager_display(&task_manager);

    /* 11. Final report. */
    printf("\n====================================\n");
    printf("       FINAL DATASET REPORT\n");
    printf("====================================\n");

    printf("Tasks completed: %d/%d\n",
           completed_count, active_workers);

    printf("Total records processed: %ld\n", total_count);
    printf("Total marks: %lld\n", total_sum);

    if (total_count > 0) {
        printf("Average marks: %.2f\n",
               (double)total_sum / total_count);

        printf("Minimum marks: %d\n", minimum);
        printf("Maximum marks: %d\n", maximum);
    }

    printf("====================================\n");

    if (completed_count == active_workers)
        printf("[Manager] Dataset analysis successful.\n");
    else
        printf("[Manager] Dataset analysis incomplete.\n");

    /* 12. Clean up shared resources. */
    shared_memory_destroy(shared, shm_fd, shm_name);

    return completed_count == active_workers ? 0 : 1;
}


/* Unix Domain Socket server: handles one client at a time. */
int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    printf("\n========== SECURE IPC SOCKET SERVER ==========\n");

    int server_fd = ipc_socket_server_create();

    if (server_fd == -1) {
        fprintf(stderr, "[Manager] Could not start socket server.\n");
        return 1;
    }

    printf("[Manager] Listening on %s\n", IPC_SOCKET_PATH);
    printf("[Manager] Allowed client UID: %d\n", getuid());

    int running = 1;

    const char *setting = getenv("SECUREIPC_ALLOW_SHUTDOWN");
    int shutdown_authorized = setting && strcmp(setting, "1") == 0;

    printf("[Security] Shutdown permission: %s\n",
           shutdown_authorized ? "ENABLED" : "DISABLED");

    printf("[Manager] Waiting for client requests...\n");

    while (running) {
        int client_fd = ipc_socket_accept_client(server_fd);

        if (client_fd == -1)
            continue;

        char request[IPC_REQUEST_SIZE];

        if (ipc_socket_authenticate_client(client_fd, getuid()) != 0) {
            ipc_socket_send_response(
                client_fd,
                "ERROR: Client authentication failed.\n"
            );
            ipc_socket_close_client(client_fd);
            continue;
        }

        if (ipc_socket_read_request(
                client_fd, request, sizeof(request)) != 0) {
            ipc_socket_send_response(
                client_fd,
                "ERROR: Invalid or oversized request.\n"
            );
            ipc_socket_close_client(client_fd);
            continue;
        }

        printf("[Manager] Received request: %s\n", request);

        if (strcmp(request, "STATUS") == 0) {
            ipc_socket_send_response(
                client_fd,
                "SUCCESS: SecureIPC manager is running.\n"
            );
        }
        else if (strcmp(request, "ANALYZE") == 0) {
            ipc_socket_send_response(
                client_fd,
                "SUCCESS: Dataset analysis started.\n"
            );

            printf("\n[Manager] Processing ANALYZE request...\n");

            int result = run_dataset_analysis();

            if (result == 0) {
                ipc_socket_send_response(
                    client_fd,
                    "SUCCESS: Dataset analysis completed.\n"
                );
            } else {
                ipc_socket_send_response(
                    client_fd,
                    "ERROR: Dataset analysis failed.\n"
                );
            }
        }
        else if (strcmp(request, "SHUTDOWN") == 0) {
            if (!shutdown_authorized) {
                ipc_socket_send_response(
                    client_fd,
                    "DENIED: Shutdown permission is disabled.\n"
                );

                printf("[Security] SHUTDOWN denied by command policy.\n");
            } else {
                ipc_socket_send_response(
                    client_fd,
                    "SUCCESS: Manager shutting down.\n"
                );

                printf("[Security] Authorized shutdown accepted.\n");
                running = 0;
            }
        }
        else {
            ipc_socket_send_response(
                client_fd,
                "ERROR: Unsupported command.\n"
            );
        }

        ipc_socket_close_client(client_fd);
    }

    ipc_socket_close_server(server_fd);

    printf("[Manager] Server stopped. Socket cleaned up.\n");
    return 0;
}
