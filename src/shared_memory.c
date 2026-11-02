
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#include "shared_memory.h"

static int lock_shared(sem_t *mutex) {
    while (sem_wait(mutex) == -1) {
        if (errno == EINTR)
            continue;

        return -1;
    }

    return 0;
}

static int unlock_shared(sem_t *mutex) {
    return sem_post(mutex);
}

SharedData *shared_memory_create(
    char *name,
    size_t name_size,
    int *shm_fd
) {
    if (!name || !shm_fd || name_size == 0)
        return NULL;

    snprintf(name, name_size, "/secureipc_%ld", (long)getpid());

    int fd = shm_open(
        name,
        O_CREAT | O_EXCL | O_RDWR,
        0600
    );

    if (fd == -1) {
        perror("shm_open");
        return NULL;
    }

    if (ftruncate(fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(name);
        return NULL;
    }

    SharedData *shared = mmap(
        NULL,
        sizeof(SharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (shared == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(name);
        return NULL;
    }

    memset(shared, 0, sizeof(SharedData));

    /* pshared = 1 enables inter-process synchronization. */
    if (sem_init(&shared->mutex, 1, 1) == -1) {
        perror("sem_init");
        munmap(shared, sizeof(SharedData));
        close(fd);
        shm_unlink(name);
        return NULL;
    }

    *shm_fd = fd;

    printf("[Manager] Shared memory initialized.\n");
    printf("[Manager] Process-shared semaphore initialized.\n");

    return shared;
}

int shared_memory_store(
    SharedData *shared,
    int task_id,
    int worker_id,
    int success,
    const DatasetStats *stats
) {
    if (!shared ||
        task_id < 1 ||
        task_id > SHARED_TASK_LIMIT ||
        (success && !stats)) {
        return -1;
    }

    if (lock_shared(&shared->mutex) == -1)
        return -1;

    SharedResult *slot = &shared->results[task_id - 1];

    slot->task_id = task_id;
    slot->worker_id = worker_id;
    slot->success = success;

    if (success)
        slot->stats = *stats;
    else
        memset(&slot->stats, 0, sizeof(slot->stats));

    return unlock_shared(&shared->mutex);
}

int shared_memory_read(
    SharedData *shared,
    int task_id,
    SharedResult *result
) {
    if (!shared ||
        !result ||
        task_id < 1 ||
        task_id > SHARED_TASK_LIMIT) {
        return -1;
    }

    if (lock_shared(&shared->mutex) == -1)
        return -1;

    *result = shared->results[task_id - 1];

    return unlock_shared(&shared->mutex);
}

void shared_memory_destroy(
    SharedData *shared,
    int shm_fd,
    const char *name
) {
    if (shared && shared != MAP_FAILED) {
        sem_destroy(&shared->mutex);
        munmap(shared, sizeof(SharedData));
    }

    if (shm_fd >= 0)
        close(shm_fd);

    if (name)
        shm_unlink(name);
}
