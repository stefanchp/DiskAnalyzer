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


void serialize_report(TreeNode *node, int depth, uint64_t total_size, char *buffer, size_t *offset, size_t max_len) {
    if (!node || *offset >= max_len - 100) return;

    // Calcul procentaj
    float percent = 0.0f;
    if (total_size > 0) percent = ((float)node->size / (float)total_size) * 100.0f;

    // Calcul bara vizuala (max 20 caractere)
    char bar[21];
    int bar_len = (int)(percent / 5.0f); 
    if (bar_len > 20) bar_len = 20;
    memset(bar, '#', bar_len);
    bar[bar_len] = '\0';

    // Formatare linie
    // Folosim indentare simpla cu "-"
    char indent[32] = "";
    for(int i=0; i<depth && i<15; i++) strcat(indent, "-");

    // Scriere in buffer la pozitia curenta (*offset)
    int written = snprintf(buffer + *offset, max_len - *offset, 
        "%s/%s %.1f%% (%lu bytes) %s\n", 
        indent, node->name, percent, node->size, bar);
    
    if (written > 0) *offset += written;

    // Recursivitate pentru copii
    TreeNode *child = node->children;
    while (child) {
        serialize_report(child, depth + 1, total_size, buffer, offset, max_len);
        child = child->next;
    }
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
            else if(job->status == JOB_DONE){ 
                strcpy(status_text, "DONE");

            }
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
    } 
    else if(req.type == CMD_REMOVE)
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
    else if(req.type == CMD_PRINT)
    {
        AnalysisJob* job = scheduler_get_job_info(req.id);
        if (job == NULL) {
            res.status_code = -1;
            snprintf(res.message, sizeof(res.message), "Job ID %d not found.", req.id);
        }
        else if (job->status != JOB_DONE) {
            res.status_code = -1;
            snprintf(res.message, sizeof(res.message), "Job ID %d is not finished yet (Status: %d).", req.id, job->status);
        }
        else if (job->result == NULL) {
            res.status_code = -1;
            snprintf(res.message, sizeof(res.message), "Job ID %d is done but result tree is NULL.", req.id);
        }
        else {
            // Job is Done, generam raportul
            size_t offset = 0;
            snprintf(res.message, sizeof(res.message), "Analysis Report for %s:\n", job->path);
            offset = strlen(res.message);
            
            // Apelam functia helper scrisa mai sus
            // Pornim cu depth 0 si total_size = dimensiunea radacinii
            serialize_report(job->result, 0, job->result->size, res.message, &offset, sizeof(res.message));
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