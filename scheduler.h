#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>
#include <stdbool.h>

#define MAX_THREADS 4
#define MAX_JOBS 100

typedef enum {
    JOB_PENDING,
    JOB_IN_PROGRESS,
    JOB_SUSPENDED,
    JOB_DONE,
    JOB_REMOVED
} JobStatus;

typedef struct {
    int id;
    char path[4096];
    int priority;      
    JobStatus status;
    float progress;    
    
    pthread_mutex_t job_lock;   
    pthread_cond_t resume_cond; 
} AnalysisJob;

typedef struct {
    AnalysisJob* jobs[MAX_JOBS];
    int count;
    pthread_mutex_t lock;      
    pthread_cond_t notify;     
    bool shutdown;             
    pthread_t workers[MAX_THREADS];
} ThreadPool;

// Functii de management al scheduler-ului
void scheduler_init();
void scheduler_destroy();

// Operatii pe joburi
int scheduler_add_job(const char *path, int priority);
int scheduler_suspend_job(int job_id);
int scheduler_resume_job(int job_id);
int scheduler_remove_job(int job_id);

// Functii de interogare
AnalysisJob* scheduler_get_job_info(int job_id);
void scheduler_list_all_jobs();

#endif