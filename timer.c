#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#define QUEUESIZE 10
#define LOOP 20
#define CONSUMERS 10

void *producer(void *args);
void *consumer(void *args);

typedef struct
{
    int buf[QUEUESIZE];
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

typedef struct
{
    queue *fifo;
    int tasksToExecute;
    int period;
} thread_arg;

pthread_mutex_t mutexProd, mutexCons;
FILE *fprod, *fcons;
int finished = 0;

queue *queueInit(void);
void queueDelete(queue *q);
void queueAdd(queue *q, int in);
void queueDel(queue *q, int *out);

void timer(pthread_t *thread, thread_arg *arg)
{
    pthread_create(thread, NULL, producer, arg);
}

int main()
{
    queue *fifo;
    pthread_t pro;
    pthread_t con[CONSUMERS];

    fifo = queueInit();
    if (fifo == NULL)
    {
        fprintf(stderr, "main: Queue Init failed.\n");
        exit(1);
    }

    thread_arg arg;
    arg.fifo = fifo;
    arg.tasksToExecute = LOOP;
    arg.period = 100000;

    // initialize files and their mutexes
    pthread_mutex_init(&mutexProd, NULL);
    fprod = fopen("dtWasteProd", "w");
    pthread_mutex_init(&mutexCons, NULL);
    fcons = fopen("dtWasteCons", "w");

    for (int i = 0; i < CONSUMERS; i++)
        pthread_create(&con[i], NULL, consumer, fifo);

    timer(&pro, &arg);

    pthread_join(pro, NULL);
    finished = 1;
    pthread_cond_broadcast(fifo->notEmpty);

    for (int i = 0; i < CONSUMERS; i++)
        pthread_join(con[i], NULL);

    queueDelete(fifo);
    // close files and destroy their mutexes
    fclose(fprod);
    fclose(fcons);
    pthread_mutex_destroy(&mutexProd);
    pthread_mutex_destroy(&mutexCons);

    return 0;
}

void *producer(void *q)
{
    thread_arg *arg;
    int i;

    arg = (thread_arg *)q;
    queue *fifo = arg->fifo;
    int tasksToExecute = arg->tasksToExecute;
    int period = arg->period;

    // create files to save dtAdd, dtWaste
    FILE *fp1;
    char fname1[20];
    snprintf(fname1, sizeof(fname1), "dtAdd_%ld.txt", (long)pthread_self());
    fp1 = fopen(fname1, "w");

    // time dtAdd
    long dtAdd;
    struct timeval queueAddCurrTime, queueAddPrevTime;

    long dtWaste = 0;

    gettimeofday(&queueAddCurrTime, NULL);
    for (i = 0; i < tasksToExecute; i++)
    {
        usleep(period - dtWaste); // fixes drift time

        pthread_mutex_lock(fifo->mut);
        while (fifo->full)
        {
            printf("producer: queue FULL.\n");
            pthread_cond_wait(fifo->notFull, fifo->mut);
        }
        // time queueAdd()
        queueAddPrevTime = queueAddCurrTime;
        gettimeofday(&queueAddCurrTime, NULL);
        dtAdd = (queueAddCurrTime.tv_sec - queueAddPrevTime.tv_sec) * 1000000 + (queueAddCurrTime.tv_usec - queueAddPrevTime.tv_usec);
        queueAdd(fifo, i);

        dtWaste = dtAdd - (period - dtWaste);
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);

        fprintf(fp1, "%ld\n", dtAdd);
        pthread_mutex_lock(&mutexProd);
        fprintf(fprod, "%ld\n", dtWaste);
        pthread_mutex_unlock(&mutexProd);
    }
    fclose(fp1);
    pthread_exit(NULL);
}

void *consumer(void *q)
{
    queue *fifo;
    int i, d;

    fifo = (queue *)q;

    while (1)
    {
        pthread_mutex_lock(fifo->mut);
        while (fifo->empty)
        {
            if (finished)
            {
                pthread_mutex_unlock(fifo->mut);
                pthread_exit(NULL);
            }
            printf("consumer: queue EMPTY.\n");
            pthread_cond_wait(fifo->notEmpty, fifo->mut);
        }
        queueDel(fifo, &d);
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notFull);
        printf("consumer: received %d.\n", d);
    }
    pthread_exit(NULL);
}

queue *queueInit(void)
{
    queue *q;

    q = (queue *)malloc(sizeof(queue));
    if (q == NULL)
        return (NULL);

    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->mut, NULL);
    q->notFull = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notEmpty, NULL);

    return (q);
}

void queueDelete(queue *q)
{
    pthread_mutex_destroy(q->mut);
    free(q->mut);
    pthread_cond_destroy(q->notFull);
    free(q->notFull);
    pthread_cond_destroy(q->notEmpty);
    free(q->notEmpty);
    free(q);
}

void queueAdd(queue *q, int in)
{
    q->buf[q->tail] = in;
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

void queueDel(queue *q, int *out)
{
    *out = q->buf[q->head];

    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;

    return;
}