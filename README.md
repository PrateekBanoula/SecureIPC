# SecureIPC – Secure Inter-Process Communication and Process Management System

**SecureIPC** is a Linux-based C project designed to demonstrate secure Inter-Process Communication (IPC), process management, process synchronization, task distribution, and fault recovery.

The system uses multiple Linux IPC mechanisms to enable communication between a manager process and worker processes. It also implements client-server communication through Unix Domain Sockets, client authentication, access control, and request validation.

The project aims to demonstrate how operating-system concepts can be combined to build a reliable and secure process-management system.

---

## 1. Project Objectives

The main objectives of SecureIPC are:

- Implement process creation and management using `fork()` and `waitpid()`.
- Demonstrate Inter-Process Communication using pipes, shared memory, and Unix Domain Sockets.
- Implement process synchronization using POSIX semaphores.
- Distribute tasks dynamically among multiple worker processes.
- Detect worker failures and recover failed tasks.
- Implement client authentication and access control.
- Validate client requests and reject unauthorized or malformed commands.
- Analyze IPC performance, synchronization overhead, and reliability.

## 2. System Architecture

SecureIPC follows a manager-worker architecture.

The Manager process is responsible for accepting client requests, authenticating clients, distributing tasks, monitoring workers, collecting results, and handling failures.

Worker processes execute assigned tasks independently and store their results in shared memory.

### Architecture Overview

```text
                   CLIENT APPLICATION
                          |
                          |
                  Unix Domain Socket
                          |
                          v
                 +------------------+
                 |  MANAGER PROCESS |
                 +------------------+
                          |
                 Authentication
                 Request Validation
                 Access Control
                          |
                          v
                 +------------------+
                 |   TASK MANAGER   |
                 +------------------+
                          |
                 Task Distribution
                          |
                Pipes (Manager → Worker)
                          |
           +--------------+--------------+
           |              |              |
           v              v              v
       Worker 1        Worker 2        Worker 3
           |              |              |
           +--------------+--------------+
                          |
                  Shared Memory
                          |
                POSIX Semaphore
                          |
                          v
                 Manager Collects
                     Results
                          |
                          v
                 Final Data Report
```

## 3. Technologies Used

| Technology | Purpose |
|---|---|
| C | Core implementation |
| Linux / Ubuntu WSL2 | Execution environment |
| GCC | Compilation |
| POSIX Processes | Process creation and management |
| Pipes | Manager-to-worker communication |
| POSIX Shared Memory | Sharing worker results |
| POSIX Semaphores | Synchronization |
| Unix Domain Sockets | Client-manager communication |
| `SO_PEERCRED` | Client credential verification |
| Python | Dataset generation and testing |
| Git / GitHub | Version control |

## 4. Implemented Features

### 4.1 Process Management

- Process creation using `fork()`.
- Parent-child process relationships.
- Process identification using `getpid()` and `getppid()`.
- Process termination monitoring using `waitpid()`.
- Exit-status verification using `WIFEXITED()` and `WEXITSTATUS()`.
- Signal-based failure detection using `WIFSIGNALED()` and `WTERMSIG()`.

### 4.2 Pipe-Based Communication

Pipes are used for communication between the manager and worker processes.

- Manager sends task information through pipes.
- Each worker receives its assigned task.
- Separate task pipes are created for individual workers.
- Pipe descriptors are closed when no longer required.

### 4.3 Shared Memory

POSIX shared memory is used to transfer worker results back to the manager.

- Shared memory creation using `shm_open()`.
- Memory mapping using `mmap()`.
- Shared result storage.
- Result retrieval by the manager.
- Shared memory cleanup using `shm_unlink()`.

### 4.4 Process Synchronization

POSIX process-shared semaphores are used to protect shared-memory operations.

- Semaphore initialization using `sem_init()`.
- Process-shared synchronization.
- Critical-section protection using `sem_wait()` and `sem_post()`.
- Semaphore cleanup using `sem_destroy()`.

### 4.5 Task Management

The Task Manager maintains task information and execution status.

Supported task states:

- PENDING
- RUNNING
- COMPLETED
- FAILED
- RETRYING

Each task maintains:

- Task ID
- Worker ID
- Assigned record range
- Execution status
- Retry count

### 4.6 Dynamic Task Distribution

The manager dynamically divides dataset records among available workers.

For example, with 100 records and three workers:

| Worker | Assigned Records |
|---|---:|
| Worker 1 | 34 |
| Worker 2 | 33 |
| Worker 3 | 33 |

This distributes the workload as evenly as possible.

### 4.7 Worker Failure Detection and Recovery

SecureIPC includes basic fault recovery.

- Worker crashes are detected using `waitpid()`.
- Failed tasks are identified.
- Failed tasks are reassigned to replacement workers.
- Retry attempts are tracked.
- Final results are collected from successful workers.

A controlled worker crash can be simulated using:

```bash
SECUREIPC_CRASH_TASK=2 ./secureipc
```

When running through the socket server, set the environment variable on the manager process.

### 4.8 Unix Domain Socket Communication

Unix Domain Sockets provide local client-server communication.

Implemented functionality:

- Socket creation using `socket()`.
- Server binding using `bind()`.
- Listening using `listen()`.
- Client connection using `connect()`.
- Connection acceptance using `accept()`.
- Request and response transmission.

The socket is created at:

```text
/tmp/secureipc.sock
```

The server currently handles one client connection at a time and supports repeated client connections.

### 4.9 Client Authentication

Client authentication is implemented using Linux `SO_PEERCRED`.

The manager obtains:

- Process ID (PID)
- User ID (UID)
- Group ID (GID)

The connecting client's UID is checked against the manager's UID.

### 4.10 Access Control and Request Validation

The system currently supports the following commands:

| Command | Description |
|---|---|
| STATUS | Checks whether the manager is running |
| ANALYZE | Starts dataset processing |
| SHUTDOWN | Requests manager shutdown |

Security measures implemented:

- Socket file permissions restricted to `0600`.
- Client UID verification.
- Invalid command rejection.
- Oversized request rejection.
- Explicit shutdown authorization setting.

Shutdown authorization can be enabled when starting the manager:

```bash
SECUREIPC_ALLOW_SHUTDOWN=1 ./secureipc
```

Without this setting, shutdown requests are denied by default.

---

## 5. Dataset Processing

SecureIPC currently uses a CSV dataset containing four columns:

| Column | Description |
|---|---|
| id | Record identifier |
| marks | Marks value |
| age | Age |
| salary | Salary |

The dataset processor:

- Reads CSV records.
- Skips the header and blank lines.
- Dynamically reads lines using `getline()`.
- Validates CSV records.
- Processes assigned record ranges.
- Calculates count, sum, minimum, and maximum marks.

The manager aggregates the worker results to generate the final report.

### Example Output

```text
========== FINAL DATASET REPORT ==========

Tasks completed: 3/3
Total records processed: 100
Total marks: [Calculated from dataset]
Average marks: [Calculated from dataset]
Minimum marks: [Calculated from dataset]
Maximum marks: [Calculated from dataset]

===========================================
```

*Note: Actual statistics depend on the dataset being processed.*

## 6. Project Structure

```text
SecureIPC/
│
├── .vscode/
│   ├── c_cpp_properties.json
│   ├── launch.json
│   └── settings.json
│
├── data/
│   └── dataset.csv
│
├── docs/
│
├── include/
│   ├── dataset.h
│   ├── ipc_socket.h
│   ├── shared_memory.h
│   ├── task_manager.h
│   └── worker.h
│
├── logs/
│
├── report/
│
├── src/
│   ├── client.c
│   ├── dataset.c
│   ├── ipc_socket.c
│   ├── manager.c
│   ├── shared_memory.c
│   ├── task_manager.c
│   └── worker.c
│
├── tests/
│
├── secureipc
├── secureipc_client
└── README.md
```

## 7. Installation and Execution

### Prerequisites

- Linux or Ubuntu (WSL2 supported)
- GCC compiler
- POSIX threads and semaphore support

### Step 1: Clone the repository

```bash
git clone <YOUR_REPOSITORY_URL>
cd SecureIPC
```

### Step 2: Compile the Manager

```bash
gcc -Wall -Wextra -std=c11 -Iinclude \
src/manager.c src/worker.c src/dataset.c \
src/task_manager.c src/shared_memory.c src/ipc_socket.c \
-o secureipc -pthread
```

### Step 3: Compile the Client

```bash
gcc -Wall -Wextra -std=c11 -Iinclude \
src/client.c src/ipc_socket.c \
-o secureipc_client
```

### Step 4: Start the Manager

```bash
./secureipc
```

To explicitly allow shutdown:

```bash
SECUREIPC_ALLOW_SHUTDOWN=1 ./secureipc
```

### Step 5: Send Client Requests

Check manager status:

```bash
./secureipc_client STATUS
```

Start dataset analysis:

```bash
./secureipc_client ANALYZE
```

Request manager shutdown (when explicitly enabled):

```bash
./secureipc_client SHUTDOWN
```

---

## 8. Security Testing

The following tests have been performed:

| Test | Result |
|---|---|
| Client authentication using SO_PEERCRED | Tested successfully |
| Socket permission verification (`0600`) | Tested successfully |
| Invalid command rejection | Tested successfully |
| Oversized request rejection | Tested successfully |
| Normal client-server communication | Tested successfully |
| Dataset analysis through socket | Tested successfully |
| Worker crash detection | Tested successfully |
| Failed task recovery | Tested successfully |
| Default shutdown denial | Tested successfully |
| Explicitly authorized shutdown | Pending verification |
| Different Linux user access restriction | Pending verification |
| Socket cleanup and restart testing | Pending verification |

## 9. Current Development Status

### Phase 1 – Core IPC and Process Management

**Status: Completed**

- Process creation and management.
- Pipe-based communication.
- Shared memory.
- Semaphore synchronization.
- Worker management.
- Dynamic task distribution.
- Failure detection and recovery.

### Phase 2 – Security and Authentication

**Status: In Progress**

- Unix Domain Socket integration.
- Client authentication.
- UID verification.
- Socket permission restriction.
- Request validation.
- Command-level access control.
- Security testing.

### Phase 3 – Advanced IPC and Dashboard

**Status: Planned**

- Multiple simultaneous clients.
- Concurrent request handling.
- Synchronized request queue.
- Web-based monitoring dashboard.
- Live worker and task monitoring.
- Execution history and log visualization.

### Phase 4 – Performance Analysis and Evaluation

**Status: Planned**

- IPC performance benchmarking.
- Synchronization overhead measurement.
- Comparison of pipes, shared memory, and Unix Domain Sockets.
- Reliability and security evaluation.
- Performance graphs and analysis.
- Final project documentation.

---

## 10. Future Enhancements

Potential future improvements include:

- Support for multiple concurrent clients.
- Thread-based or event-driven request handling.
- Web dashboard for real-time monitoring.
- Detailed logging and audit trails.
- More advanced authentication and authorization.
- Performance benchmarking for different IPC mechanisms.
- Improved resource isolation and process supervision.

## 11. Learning Outcomes

This project demonstrates practical implementation of:

- Linux process management.
- Inter-Process Communication.
- Process synchronization.
- Shared-memory coordination.
- Client-server architecture.
- Unix Domain Sockets.
- Authentication and access control.
- Fault detection and recovery.
- Operating-system resource management.

---

## 12. Project Information

**Project Name:** SecureIPC – Secure Inter-Process Communication and Process Management System

**Domain:** Operating Systems, System Programming, Inter-Process Communication, and Computer Security

**Language:** C

**Platform:** Linux / Ubuntu (WSL2 supported)

**Development Status:** Phase 2 – Security and Authentication

---

*SecureIPC is an academic project developed to explore and demonstrate practical operating-system concepts through a modular, process-based system.*
