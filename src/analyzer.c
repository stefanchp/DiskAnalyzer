#define _XOPEN_SOURCE 700 // POSIX.1-2008, daca scot da crash
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "analyzer.h"
#include "scheduler.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>


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

    if (p < 0.0f) p = 0.0f;
    if (p > 100.0f) p = 100.0f;

    pthread_mutex_lock(&job->job_lock);
    job->progress = p;
    pthread_mutex_unlock(&job->job_lock);
}

static int join_path(char *out, size_t out_sz, const char *dir, const char *name) 
{
    if (!out || !dir || !name) return -1;

    size_t dl = strlen(dir);
    int need_slash = (dl > 0 && dir[dl - 1] != '/');

    int n = snprintf(out, out_sz, need_slash ? "%s/%s" : "%s%s", dir, name);
    if (n < 0 || (size_t)n >= out_sz) return -1;
    return 0;
}

static int is_dot_or_dotdot(const char *name) 
{
    return (strcmp(name, ".") == 0 || strcmp(name, "..") == 0);
}

static int scan_dir_recursive(const char *path, TreeNode *parent, AnalysisJob *job) 
{
    if (job_wait_if_suspended(job) || job_should_stop(job))
        return 1;

    DIR *d = opendir(path);
    if (!d) 
        return 0;
    if (job_wait_if_suspended(job) || job_should_stop(job))
    {
        closedir(d);
        return 1;
    }
        
    struct dirent *ent;
    while (1) 
    {
        errno = 0;
        ent = readdir(d);
        if (ent == NULL)
            break;
        if (job_wait_if_suspended(job) || job_should_stop(job)) 
        {
            closedir(d);
            return 1;
        }

        if (is_dot_or_dotdot(ent->d_name)) 
            continue;
        
        char fullpath[PATH_MAX];
        if (join_path(fullpath, sizeof(fullpath), path, ent->d_name) != 0)
            continue;

        struct stat st;
        if (lstat(fullpath, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode)) 
        {
            TreeNode *child = create_node(ent->d_name, NODE_DIR);
            if (!child) 
            {
                closedir(d);
                return 1;
            }
            add_child(parent, child);
            job_inc_dirs(job);

            int rc = scan_dir_recursive(fullpath, child, job);
            if (rc != 0) 
            {
                closedir(d);
                return 1;
            }

            parent->size += child->size;

        } 
        else if (S_ISREG(st.st_mode)) 
        {
            TreeNode *child = create_node(ent->d_name, NODE_FILE);
            if (!child) 
            {
                closedir(d);
                return 1;
            }

            child->size = (uint64_t)st.st_size;
            add_child(parent, child);
            job_inc_files(job);

            parent->size += child->size;

        } 
        else if (S_ISLNK(st.st_mode)) 
        {
            TreeNode *child = create_node(ent->d_name, NODE_FILE);
            if (!child) 
            {
                closedir(d);
                return 1;
            }

            child->size = (uint64_t)st.st_size;
            add_child(parent, child);
            job_inc_files(job);

            parent->size += child->size;

        } 
        else 
        {
            TreeNode *child = create_node(ent->d_name, NODE_FILE);
            if (!child) 
            {
                closedir(d);
                return 1;
            }
            child->size = 0;
            add_child(parent, child);
            job_inc_files(job);
        }

        if (job) 
        {
            pthread_mutex_lock(&job->job_lock);
            uint64_t f = job->files_scanned;
            pthread_mutex_unlock(&job->job_lock);

            if (f != 0 && (f % 200 == 0))
            {
                float p = 1.0f + (float)(f % 10000) / 200.0f;
                if (p > 99.0f) p = 99.0f;
                job_set_progress(job, p);
            }
        }
    }
    if (errno != 0) 
    {
        closedir(d);
        return 1;
    }

    closedir(d);
    return 0;
}

static const char *base_name(const char *p) 
{
    if (!p) return "";
    const char *slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
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

    TreeNode *root = create_node(base_name(path), NODE_DIR);
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
