#define _XOPEN_SOURCE 700

#include "analyzer.h"
#include "scheduler.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


static int job_should_stop(AnalysisJob *job) 
{
    return (job && job->status == JOB_REMOVED) ? 1 : 0;
}

static int job_wait_if_suspended(AnalysisJob *job) 
{
    if (!job) return 0;
    pthread_mutex_lock(&job->job_lock);

    while (job->status == JOB_SUSPENDED) 
       pthread_cond_wait(&job->resume_cond, &job->job_lock);
    
    int stop = (job->status == JOB_REMOVED);
    pthread_mutex_unlock(&job->job_lock);
    return stop;
}

static void job_inc_files(AnalysisJob *job) 
{
    if (!job) return;
    pthread_mutex_lock(&job->job_lock);
    job->files_scanned++;
    pthread_mutex_unlock(&job->job_lock);
}

static void job_inc_dirs(AnalysisJob *job) 
{
    if (!job) return;
    pthread_mutex_lock(&job->job_lock);
    job->dirs_scanned++;
    pthread_mutex_unlock(&job->job_lock);
}

static void job_set_progress(AnalysisJob *job, float p) 
{
    if (!job) return;
    pthread_mutex_lock(&job->job_lock);
    job->progress = p;
    pthread_mutex_unlock(&job->job_lock);
}

static int scan_dir_recursive(const char *path, TreeNode *parent, AnalysisJob *job) 
{
    (void)path;
    (void)parent;
    (void)job;
    return 0;
}

TreeNode *analyze_directory(const char *path, AnalysisJob *job) 
{
    if (!path || !*path) return NULL;

    if (job) 
    {
        pthread_mutex_lock(&job->job_lock);
        if (job->status == JOB_REMOVED) 
        {
            pthread_mutex_unlock(&job->job_lock);
            return NULL;
        }
        job->progress = 0.0f;
        job->files_scanned = 0;
        job->dirs_scanned = 0;
        pthread_mutex_unlock(&job->job_lock);
    }

    TreeNode *root = create_node(path, NODE_DIR);
    if (!root) return NULL;

    job_inc_dirs(job);

    if (job_wait_if_suspended(job) || job_should_stop(job))
    {
        free_tree(root);
        return NULL;
    }

    int rc = scan_dir_recursive(path, root, job);

    if (job_should_stop(job) || rc != 0) 
    {
        free_tree(root);
        return NULL;
    }

    job_set_progress(job, 100.0f);

    if (job) 
    {
        pthread_mutex_lock(&job->job_lock);
        job->result = root;
        pthread_mutex_unlock(&job->job_lock);
    }
    return root;
}