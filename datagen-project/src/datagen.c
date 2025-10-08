// datagen.c - Coordinador + Generadores (POSIX, Linux)
// Uso: ./bin/datagen -n <generadores> -m <total_registros> -o <salida.csv>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#define MAX_STR 128
#define NAME_MAXLEN 128

typedef struct {
    int id;
    char f1[MAX_STR];
    int f2;
    double f3;
} record_t;

typedef struct {
    record_t slot;
    int next_id;
    int written;
    int m_total;
    int stop;
} shm_t;

// nombres únicos por UID + PID
static char SHM_NAME[NAME_MAXLEN];
static char SEM_EMPTY[NAME_MAXLEN];
static char SEM_FULL[NAME_MAXLEN];
static char SEM_MX_REC[NAME_MAXLEN];
static char SEM_MX_IDS[NAME_MAXLEN];

static int fd_shm = -1;
static shm_t *g = NULL;
static sem_t *sem_empty = NULL, *sem_full = NULL, *sem_mx_rec = NULL, *sem_mx_ids = NULL;

static pid_t *children = NULL;
static int n_children = 0;
static FILE *csv = NULL;

// --------------------------------------------------------------

static void help(const char *prog){
    fprintf(stderr,
        "Uso: %s -n <generadores> -m <total_registros> -o <salida.csv>\n"
        "Ejemplo: %s -n 4 -m 50000 -o salida.csv\n", prog, prog);
}

static void randomize_record(record_t *r){
    static const char* words[] = {"APPLE","PEACH","MANGO","KIWI","BANANA","ORANGE","GRAPE","MELON"};
    int w = rand() % (int)(sizeof(words)/sizeof(words[0]));
    snprintf(r->f1, MAX_STR, "%s", words[w]);
    r->f2 = rand()%1000;
    r->f3 = (double)(rand()%100000)/1000.0;
}

static void make_names(){
    uid_t u = getuid(); pid_t p = getpid();
    snprintf(SHM_NAME, NAME_MAXLEN, "/shm_rec_%u_%d", (unsigned)u, (int)p);
    snprintf(SEM_EMPTY, NAME_MAXLEN, "/sem_empty_%u_%d", (unsigned)u, (int)p);
    snprintf(SEM_FULL,  NAME_MAXLEN, "/sem_full_%u_%d",  (unsigned)u, (int)p);
    snprintf(SEM_MX_REC,NAME_MAXLEN, "/sem_mx_rec_%u_%d",(unsigned)u, (int)p);
    snprintf(SEM_MX_IDS,NAME_MAXLEN, "/sem_mx_ids_%u_%d",(unsigned)u, (int)p);
}

static void init_ipc(int m_total){
    fd_shm = shm_open(SHM_NAME, O_CREAT|O_RDWR, 0660);
    if (fd_shm < 0){ perror("shm_open"); exit(1); }
    if (ftruncate(fd_shm, sizeof(shm_t)) < 0){ perror("ftruncate"); exit(1); }
    g = (shm_t*)mmap(NULL, sizeof(shm_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (g == MAP_FAILED){ perror("mmap"); exit(1); }
    memset(g, 0, sizeof(*g));
    g->next_id = 1; g->written = 0; g->m_total = m_total; g->stop = 0;

    sem_empty = sem_open(SEM_EMPTY, O_CREAT|O_EXCL, 0660, 1);
    sem_full  = sem_open(SEM_FULL,  O_CREAT|O_EXCL, 0660, 0);
    sem_mx_rec= sem_open(SEM_MX_REC,O_CREAT|O_EXCL, 0660, 1);
    sem_mx_ids= sem_open(SEM_MX_IDS,O_CREAT|O_EXCL, 0660, 1);
    if (sem_empty==SEM_FAILED||sem_full==SEM_FAILED||sem_mx_rec==SEM_FAILED||sem_mx_ids==SEM_FAILED){
        perror("sem_open"); exit(1);
    }
}
static void close_ipc_handles(){
    if (sem_empty && sem_empty!=SEM_FAILED){ sem_close(sem_empty); sem_empty=NULL; }
    if (sem_full  && sem_full !=SEM_FAILED){ sem_close(sem_full ); sem_full =NULL; }
    if (sem_mx_rec&& sem_mx_rec!=SEM_FAILED){ sem_close(sem_mx_rec); sem_mx_rec=NULL; }
    if (sem_mx_ids&& sem_mx_ids!=SEM_FAILED){ sem_close(sem_mx_ids); sem_mx_ids=NULL; }
    if (g && g!=(void*)-1){ munmap(g, sizeof(*g)); g=NULL; }
    if (fd_shm>=0){ close(fd_shm); fd_shm=-1; }
    if (csv){ fclose(csv); csv=NULL; }
}
static void unlink_ipc_names(){
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    sem_unlink(SEM_MX_REC);
    sem_unlink(SEM_MX_IDS);
}
static void cleanup_ipc(void){ close_ipc_handles(); unlink_ipc_names(); }

static void on_signal(int sig){ (void)sig; if (g) g->stop = 1; }

static void coordinator_loop(const char* csv_path){
    csv = fopen(csv_path, "w");
    if (!csv){ perror("fopen csv"); exit(1); }
    fprintf(csv, "id,field1,field2,field3\n"); // encabezado

    while (!g->stop && g->written < g->m_total){
        if (sem_wait(sem_full) == -1){ if (errno==EINTR) continue; perror("sem_wait(full)"); break; }
        if (sem_wait(sem_mx_rec) == -1){ if (errno==EINTR){ sem_post(sem_full); continue; } perror("sem_wait(mx_rec)"); break; }
        record_t local = g->slot;
        sem_post(sem_mx_rec);
        sem_post(sem_empty);

        fprintf(csv, "%d,%s,%d,%.3f\n", local.id, local.f1, local.f2, local.f3);
        g->written++;
    }
}

static void generator_loop(int idx){
    srand(time(NULL) ^ (getpid()<<16) ^ (idx*1337));
    for (;;){
        if (g->stop) break;
        if (sem_wait(sem_mx_ids) == -1){ if (errno==EINTR) continue; perror("sem_wait(mx_ids)"); break; }
        int base = g->next_id;
        if (base > g->m_total){ sem_post(sem_mx_ids); break; }
        int count = 10;
        if (base + count - 1 > g->m_total) count = g->m_total - base + 1;
        g->next_id += 10;
        sem_post(sem_mx_ids);

        for (int i=0; i<count; i++){
            if (g->stop) break;
            record_t r; r.id = base + i; randomize_record(&r);
            if (sem_wait(sem_empty) == -1){ if (errno==EINTR){ i--; continue; } perror("sem_wait(empty)"); goto out; }
            if (sem_wait(sem_mx_rec) == -1){ if (errno==EINTR){ sem_post(sem_empty); i--; continue; } perror("sem_wait(mx_rec)"); goto out; }
            g->slot = r;
            sem_post(sem_mx_rec);
            sem_post(sem_full);
        }
    }
out: _exit(0);
}

int main(int argc, char **argv){
    int n=0, m=0; const char* out = NULL; int opt;
    while ((opt = getopt(argc, argv, "n:m:o:h")) != -1){
        switch (opt){
            case 'n': n = atoi(optarg); break;
            case 'm': m = atoi(optarg); break;
            case 'o': out = optarg; break;
            case 'h': default: help(argv[0]); return (opt=='h')?0:1;
        }
    }
    if (n<=0 || m<=0 || !out){ help(argv[0]); return 1; }

    make_names(); init_ipc(m);
    signal(SIGINT, on_signal); signal(SIGTERM, on_signal);

    children = calloc(n, sizeof(pid_t)); n_children = n;
    for (int i=0;i<n;i++){
        pid_t pid=fork();
        if (pid<0){ perror("fork"); return 2; }
        if (pid==0){ generator_loop(i+1); }
        children[i]=pid;
    }
    coordinator_loop(out);
    if (g) g->stop = 1; sem_post(sem_full); sem_post(sem_empty);
    for (int i=0;i<n;i++){ if (children[i]>0) waitpid(children[i], NULL, 0); }
    cleanup_ipc(); free(children);
    return 0;
}
