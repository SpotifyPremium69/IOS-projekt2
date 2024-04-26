// Projekt IOS 2
// Autor: Jakub Smička
// Datum: 24.4.2024

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <semaphore.h>
#include <stdbool.h>
#include <sys/wait.h>

sem_t boardingS;
sem_t departureS;
sem_t changeMemoryS;
sem_t allboardedS;
sem_t busS;

int L, Z, K, TL, TB;
int L_notskiing = 0;

#define MAX_STOPS 10
#define SHARED_MEMORY_KEY 1234

typedef struct
{
    int waitingAtStop[MAX_STOPS];
    int onboardCount;
    int currentStop;
} SharedMemory;

void BusProcess(SharedMemory *shM)
{
    printf("BUS: started\n");
    int idZ = 1; // Initial stop identification

    while (idZ <= Z)
    {
        int semStatus;
        bool pickup;
        sem_getvalue(&busS, &semStatus);
        if (semStatus > 0)
        {
            sem_wait(&busS);
        }
        sem_wait(&boardingS);
        usleep(rand() % TB); // Sleep for a random time in the interval <0,TB>
        sem_wait(&changeMemoryS);
        printf("BUS: arrived to %d\n", idZ);
        shM->currentStop = idZ;
        if (shM->waitingAtStop[idZ] != 0)
        {
            pickup = true;
        }
        sem_post(&changeMemoryS);
        sem_post(&boardingS);
        sem_post(&busS);

        if (pickup == true)
        {
            sem_wait(&allboardedS);
        }
        sem_wait(&busS);
        printf("BUS: leaving %d\n", idZ);

        idZ++; // Move to the next stop
    }
    usleep(rand() % TB);
    printf("BUS: arrived to final\n");
    // Allow all skiers on bus go skiing
    printf("BUS: leaving final\n");

    printf("BUS: finish\n");
}
void SkierProcess(int idL, SharedMemory *shM)
{
    int semStatus;
    bool boarded = false;
    printf("L %d: started\n", idL);
    usleep(rand() % TL);
    int idZ = ((rand() % Z) + 1);

    sem_wait(&changeMemoryS);
    shM->waitingAtStop[idZ]++;
    printf("L %d: arrived to %d\n", idL, idZ);
    sem_post(&changeMemoryS);
    while (boarded == false)
    {
        sem_wait(&boardingS);
        sem_wait(&changeMemoryS);
        if (shM->currentStop == idZ)
        {
            sem_getvalue(&allboardedS, &semStatus);
            if (semStatus > 0)
            {
                sem_wait(&allboardedS);
            }
            if (shM->onboardCount < K)
            {
                shM->onboardCount++;
                shM->waitingAtStop[idZ]--;
                boarded = true;
            }
            if (shM->onboardCount == K || shM->waitingAtStop[idZ] == 0)
            {
                sem_post(&allboardedS);
            }
        }
        sem_post(&changeMemoryS);
        sem_post(&boardingS);
    }
    printf("L %d: boarding\n", idL);
    // waiting to be driven to final

    sem_wait(&changeMemoryS);
    shM->onboardCount--;
    printf("L %d: going to ski\n", idL);
    L_notskiing--;
    sem_post(&changeMemoryS);
}

SharedMemory *createSharedMemory()
{
    int sharedMemoryId;
    SharedMemory *sharedMemory;

    // Shared memory allocation
    sharedMemoryId = shmget(SHARED_MEMORY_KEY, sizeof(SharedMemory), IPC_CREAT | 0666);
    if (sharedMemoryId == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Shared memory attachment
    sharedMemory = (SharedMemory *)shmat(sharedMemoryId, NULL, 0);
    if (sharedMemory == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Memory initialization
    for (int i = 1; i <= Z; ++i)
    {
        sharedMemory->waitingAtStop[i] = 0;
    }
    sharedMemory->onboardCount = 0;
    sharedMemory->currentStop = 0;

    return sharedMemory;
}
void initializeSemaphores()
{
    // Initialize boarding semaphore with initial value 1
    if (sem_init(&boardingS, 0, 1) == -1)
    {
        perror("sem_init");
        exit(1);
    }

    // Initialize departure semaphore with initial value 1
    if (sem_init(&departureS, 0, 1) == -1)
    {
        perror("sem_init");
        exit(1);
    }
    if (sem_init(&changeMemoryS, 0, 1) == -1)
    {
        perror("sem_init");
        exit(1);
    }
    if (sem_init(&allboardedS, 0, 1) == -1)
    {
        perror("sem_init");
        exit(1);
    }
    if (sem_init(&busS, 0, 1) == -1)
    {
        perror("sem_init");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    // Kontrola, jestli je dostatek argumentu spusteni
    if (argc != 6)
    {
        printf("Error: Nedostatek argumentu. Pouzijte: ./proj2 L Z K TL TB\n");
        return 1;
    }
    // Kontrola, jestli jsou vsechny argumenty int a nacteni do promennych
    if (sscanf(argv[1], "%d", &L) != 1 ||
        sscanf(argv[2], "%d", &Z) != 1 ||
        sscanf(argv[3], "%d", &K) != 1 ||
        sscanf(argv[4], "%d", &TL) != 1 ||
        sscanf(argv[5], "%d", &TB) != 1)
    {
        printf("Error: Jeden z argumentu neni cislo!\n");
        return 1;
    }
    // Kontrola omezeni parametru podle zadani
    if (L < 0 || L >= 20000)
    {
        printf("Error: Neplatny pocet lyzaru!\n");
        return 1;
    }
    if (Z < 1 || Z > 10)
    {
        printf("Error: Neplatny pocet zastavek!\n");
        return 1;
    }
    if (K < 10 || K > 100)
    {
        printf("Error: Neplatna kapacita skibusu!\n");
        return 1;
    }
    if (TL < 0 || TL > 10000)
    {
        printf("Error: Neplatny cas cekani lyzare!\n");
        return 1;
    }
    if (TB < 0 || TB > 1000)
    {
        printf("Error: Neplatna doba jizdy mezi zastavkami!\n");
        return 1;
    }
    srand(time(NULL));
    L_notskiing = L;
    SharedMemory *shM = createSharedMemory();
    initializeSemaphores();
    pid_t busPid = fork();
    if (busPid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (busPid == 0)
    {
        // Child process: BusProcess
        BusProcess(shM);
        exit(EXIT_SUCCESS);
    }
    // Fork processes for SkierProcess
    for (int i = 1; i <= L; ++i)
    {
        pid_t skierPid = fork();
        if (skierPid == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (skierPid == 0)
        {
            // Child process: SkierProcess
            SkierProcess(i, shM);
            exit(EXIT_SUCCESS);
        }
    }
    printf("check\n");
    for (int i = 0; i < L + 1; ++i)
    {
        wait(NULL); // Wait for any child process to finish
    }
    shmdt(shM);

    // destroySemaphores();
    return 0;
}