#include "common.h"
#include "ipc.h"
#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>


int server_fd;

void cleanup_and_exit(int signum)
{
    scheduler_destroy();

    close(server_fd);
    unlink(SOCKET_PATH);
    exit(0);
}

void daemonize()
{
    pid_t pid =fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /*
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
    */
}

void process_client(int client_fd)
{
    request_t req;
    response_t res;

    read(client_fd, &req, sizeof(req));


    res.status_code = 0;
    memset(res.message, 0, sizeof(res.message));
    if(req.type == CMD_ADD)
    {
        int job_id = scheduler_add_job(req.path, req.priority);
        if (job_id >= 0)
        {
            snprintf(res.message, sizeof(res.message), "Job added with ID: %d", job_id);
        }
        else if (job_id <= -1000)
        {
            int conflict_id = -(job_id + 1000);
            snprintf(res.message, sizeof(res.message), "Path conflict with existing job ID: %d", conflict_id);
            res.status_code = -1;
        }
        else
        {
            snprintf(res.message, sizeof(res.message), "Failed to add job.");
            res.status_code = -1;
        }
    }
    else if(req.type == CMD_INFO)
    {
        AnalysisJob* job = scheduler_get_job_info(req.id);
        if (job != NULL)
        {
            char status_text[20];
            if(job->status == JOB_PENDING) strcpy(status_text, "PENDING");
            else if(job->status == JOB_IN_PROGRESS) strcpy(status_text, "IN_PROGRESS");
            else if(job->status == JOB_SUSPENDED) strcpy(status_text, "SUSPENDED");
            else if(job->status == JOB_DONE) strcpy(status_text, "DONE");
            else if(job->status == JOB_REMOVED) strcpy(status_text, "REMOVED");
            else strcpy(status_text, "UNKNOWN");
            snprintf(res.message, sizeof(res.message), "ID: %d, Path: %s, Priority: %d, Status: %s, Progress: %.2f%%",
                     job->id, job->path, job->priority, status_text, job->progress);
        }
        else
        {
            snprintf(res.message, sizeof(res.message), "Job ID %d not found.", req.id);
            res.status_code = -1;
        }
    }
    else if(req.type == CMD_SUSPEND)
    {
        int ret = scheduler_suspend_job(req.id);
        if (ret == 0)
        {
            snprintf(res.message, sizeof(res.message), "Job %d suspended.", req.id);
        }
        else
        {
            snprintf(res.message, sizeof(res.message), "Failed to suspend job %d.", req.id);
            res.status_code = -1;
        }
    }
    else if(req.type == CMD_RESUME)
    {
        int ret = scheduler_resume_job(req.id);
        if (ret == 0)
        {
            snprintf(res.message, sizeof(res.message), "Job %d resumed.", req.id);
        }
        else
        {
            snprintf(res.message, sizeof(res.message), "Failed to resume job %d.", req.id);
            res.status_code = -1;
        }
    } else if(req.type == CMD_REMOVE)
    {
        int ret = scheduler_remove_job(req.id);
        if (ret == 0)
        {
            snprintf(res.message, sizeof(res.message), "Job %d removed.", req.id);
        }
        else
        {
            snprintf(res.message, sizeof(res.message), "Failed to remove job %d.", req.id);
            res.status_code = -1;
        }
    }
    else if(req.type == CMD_LIST)
    {
        char buffer[100];
        strcpy(res.message, "Job List:\n");
        for (int i = 0; i < 100; i++) {
            AnalysisJob* job = scheduler_get_job_info(i);
            if (job != NULL && job->status != JOB_REMOVED) {
                snprintf(buffer, sizeof(buffer), "ID: %d, Path: %s, Priority: %d, Status: %d, Progress: %.2f%%\n",
                         job->id, job->path, job->priority, job->status, job->progress);
                if (strlen(res.message) + strlen(buffer) < sizeof(res.message)) {
                    strcat(res.message, buffer);
                }
            }
        }
    }
    else
    {
        res.status_code = -1;
        snprintf(res.message, sizeof(res.message), "Unknown command.");
    }
    write(client_fd, &res, sizeof(res));
    close(client_fd);
}

int main()
{
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);


    // daemonize();

    scheduler_init();
    server_fd = create_server_socket();


    while (1)
    {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd >= 0)
        {
            process_client(client_fd);
        }
    }

    return 0;
}