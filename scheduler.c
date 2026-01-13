#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scheduler.h"

static ThreadPool pool;

//Gaseste jobul cu prioritatea cea mai mare si cel mai vechi din coada
static int find_next_job() {
    int best_idx = -1;
    int max_prio = -1;

    for (int i = 0; i < pool.count; i++) {
        if (pool.jobs[i]->status == JOB_PENDING) {
            if (pool.jobs[i]->priority > max_prio) {
                max_prio = pool.jobs[i]->priority;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

//Gaseste conflicte doar cu subdirectoare
static int check_path_conflict(const char *new_path) {
    for (int i = 0; i < pool.count; i++) {
        if (pool.jobs[i]->status == JOB_REMOVED) continue;

        const char *existing = pool.jobs[i]->path;
        size_t len = strlen(existing);

        // Verificăm dacă new_path începe cu existing_path
        if (strncmp(new_path, existing, len) == 0) {
            // Caz 1: Sunt identice (ex: /home/user și /home/user)
            // Caz 2: Este subdirector (ex: /home/user/repo și /home/user)
            // Verificăm dacă după prefix urmează un '/' sau dacă s-a terminat string-ul
            if (new_path[len] == '\0' || new_path[len] == '/') {
                return pool.jobs[i]->id; // Returnăm ID-ul conflictului
            }
        }
    }
    return -1; // Niciun conflict găsit
}

// Functie care trebuie modificata la integrare cu analiza in sine
static void* worker_routine(void* arg) {
    while (1) {
        pthread_mutex_lock(&pool.lock);

        while (pool.count == 0 && !pool.shutdown) {
            pthread_cond_wait(&pool.notify, &pool.lock);
        }

        if (pool.shutdown) {
            pthread_mutex_unlock(&pool.lock);
            break;
        }

        int idx = find_next_job();
        if (idx == -1) {
            pthread_mutex_unlock(&pool.lock);
            usleep(10000); 
            continue;
        }

        AnalysisJob* job = pool.jobs[idx];
        pthread_mutex_lock(&job->job_lock);
        job->status = JOB_IN_PROGRESS;
        pthread_mutex_unlock(&job->job_lock);
        
        pthread_mutex_unlock(&pool.lock);

        //Functie Chiper
        analyze_directory(job->path,job);

        pthread_mutex_lock(&job->job_lock);
        if (job->status != JOB_REMOVED) {
            job->status = JOB_DONE;
            job->progress = 100.0;
            printf("Job %d (priority: %d) was finished\n", job->id, job->priority);
            fflush(stdout);
        }
        pthread_mutex_unlock(&job->job_lock);
    }
    return NULL;
}

void scheduler_init() {
    pool.count = 0;
    pool.shutdown = false;
    pthread_mutex_init(&pool.lock, NULL);
    pthread_cond_init(&pool.notify, NULL);

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&pool.workers[i], NULL, worker_routine, NULL);
    }
    printf("[Scheduler] Initializat cu %d thread-uri.\n", MAX_THREADS);
}

int scheduler_add_job(const char *path, int priority) {
    pthread_mutex_lock(&pool.lock);

    // 1. Verificăm conflictul de path (Overlap)
    int conflict_id = check_path_conflict(path);
    if (conflict_id != -1) {
        pthread_mutex_unlock(&pool.lock);
        return -(conflict_id + 1000); 
    }

    // 2. Căutăm un slot liber sau unul care poate fi reciclat
    int target_idx = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        // Dacă am găsit un loc gol (NULL) sau un job care nu mai este necesar
        if (pool.jobs[i] == NULL || pool.jobs[i]->status == JOB_REMOVED) {
            target_idx = i;
            break;
        }
    }

    // Dacă nu am găsit niciun loc disponibil în toată tabela de 100
    if (target_idx == -1) {
        pthread_mutex_unlock(&pool.lock);
        return -2005; // Coada este cu adevărat plină
    }

    // 3. Dacă slotul conținea un job vechi, eliberăm memoria lui înainte de refolosire
    if (pool.jobs[target_idx] != NULL) {
        pthread_mutex_destroy(&pool.jobs[target_idx]->job_lock);
        pthread_cond_destroy(&pool.jobs[target_idx]->resume_cond);
        free(pool.jobs[target_idx]);
    }

    // 4. Alocăm noul job în slotul găsit
    AnalysisJob* nj = malloc(sizeof(AnalysisJob));
    nj->id = target_idx; // ID-ul devine indexul slotului (foarte eficient)
    strncpy(nj->path, path, 4096);
    nj->priority = (priority < 1) ? 1 : (priority > 3 ? 3 : priority);
    nj->status = JOB_PENDING;
    nj->progress = 0.0;
    
    pthread_mutex_init(&nj->job_lock, NULL);
    pthread_cond_init(&nj->resume_cond, NULL);

    pool.jobs[target_idx] = nj;
    
    // Incrementăm count doar dacă am adăugat la finalul listei
    if (target_idx >= pool.count) {
        pool.count = target_idx + 1;
    }

    pthread_cond_signal(&pool.notify);
    pthread_mutex_unlock(&pool.lock);
    
    return target_idx;
}

int scheduler_suspend_job(int job_id) {
    if (job_id < 0 || job_id >= pool.count) return -1;
    pthread_mutex_lock(&pool.jobs[job_id]->job_lock);
    if (pool.jobs[job_id]->status == JOB_IN_PROGRESS) {
        pool.jobs[job_id]->status = JOB_SUSPENDED;
        printf("[Scheduler] Job %d suspendat.\n", job_id);
    }
    pthread_mutex_unlock(&pool.jobs[job_id]->job_lock);
    return 0;
}

int scheduler_resume_job(int job_id) {
    if (job_id < 0 || job_id >= pool.count) return -1;
    pthread_mutex_lock(&pool.jobs[job_id]->job_lock);
    if (pool.jobs[job_id]->status == JOB_SUSPENDED) {
        pool.jobs[job_id]->status = JOB_IN_PROGRESS;
        pthread_cond_signal(&pool.jobs[job_id]->resume_cond);
        printf("[Scheduler] Job %d reluat.\n", job_id);
    }
    pthread_mutex_unlock(&pool.jobs[job_id]->job_lock);
    return 0;
}

int scheduler_remove_job(int job_id) {
    if (job_id < 0 || job_id >= pool.count) return -1;
    pthread_mutex_lock(&pool.jobs[job_id]->job_lock);
    pool.jobs[job_id]->status = JOB_REMOVED;
    pthread_cond_signal(&pool.jobs[job_id]->resume_cond);
    pthread_mutex_unlock(&pool.jobs[job_id]->job_lock);
    return 0;
}

AnalysisJob* scheduler_get_job_info(int job_id) {
    if (job_id < 0 || job_id >= pool.count) return NULL;
    return pool.jobs[job_id];
}

void scheduler_destroy() {
    pthread_mutex_lock(&pool.lock);
    pool.shutdown = true;
    pthread_cond_broadcast(&pool.notify);
    pthread_mutex_unlock(&pool.lock);

    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(pool.workers[i], NULL);
    }

    for (int i = 0; i < pool.count; i++) {
        pthread_mutex_destroy(&pool.jobs[i]->job_lock);
        pthread_cond_destroy(&pool.jobs[i]->resume_cond);
        free(pool.jobs[i]);
    }
    pthread_mutex_destroy(&pool.lock);
    pthread_cond_destroy(&pool.notify);
    printf("[Scheduler] Sistem oprit si resurse eliberate.\n");
}
