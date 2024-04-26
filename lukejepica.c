// PROJEKT 2
// Author: Lukáš Gronich
// Date 24.04.2024

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <stdbool.h>

#include <sys/wait.h>
#include <time.h>
#include <asm-generic/fcntl.h>
// #include "proj2.h"

#define MAX_WAITING_TIME 10
#define MAX_BUS_RIDE_TIME 1000
#define MAX_STATIONS 10
#define MAX_CAPACITY 100

#define SEM_KEY 1234
#define SHM_KEY 5678

// Semafory
sem_t mutex;        // Synchronizace výstupu
sem_t *capac;
sem_t *leaving_bus;  // Notifikace příjezdu autobusu
sem_t *bus_finish; // Notifikace odjezdu autobusu
sem_t *boardingS;
sem_t *station_number[10];

int sem_id; // ID semaforu
int shm_id; // ID sdílené paměti
int Z;

typedef struct
{
    // Zde můžete definovat sdílené proměnné potřebné pro synchronizaci
    int skiers_waiting[MAX_STATIONS]; // Počet lyžařů čekajících na každé zastávce
    int skiers_station[MAX_STATIONS];
    int skiers_boarded;               // Počet lyžařů, kteří už nastoupili do autobusu
    int current_station;
    int event_number;
    // Další proměnné podle potřeby
} shared_memory;

void initialize_semaphore()
{
    if (sem_init(&mutex, 1, 1) == -1)
    {
        perror("sem_init");
        exit(1);
    }
    
    capac = sem_open("/capac", O_CREAT | O_EXCL, 0644, 0);
    if (capac == SEM_FAILED)
    {
        capac = sem_open("/capac", 0);
        if (capac == SEM_FAILED)
        {
            perror("Chyba při otevírání semaforu capac");
            exit(1);
        }
    }

    leaving_bus = sem_open("/leaving_bus", O_CREAT | O_EXCL, 0644, 0);
    if (leaving_bus == SEM_FAILED)
    {
        leaving_bus = sem_open("/leaving_bus", 0);
        if (leaving_bus == SEM_FAILED)
        {
            perror("Chyba při otevírání semaforu leaving_bus");
            exit(1);
        }
    }
    
    bus_finish = sem_open("/bus_finish", O_CREAT | O_EXCL, 0644, 0);
    if (boardingS == SEM_FAILED)
    {
        bus_finish = sem_open("/bus_finish", 0);
        if (bus_finish == SEM_FAILED)
        {
            perror("Chyba při otevírání semaforu bus_finish");
            exit(1);
        }
    }
    
    boardingS = sem_open("/boardingS", O_CREAT | O_EXCL, 0644, 0);
    if (boardingS == SEM_FAILED)
    {
        boardingS = sem_open("/boardingS", 0);
        if (boardingS == SEM_FAILED)
        {
            perror("Chyba při otevírání semaforu boardingS");
            exit(1);
        }
    }

    for (int i = 0; i < 10; i++){
        char sem_name[20];
        sprintf(sem_name, "/station%d_sem", i + 1);

        station_number[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 0);

        if(station_number[i] == SEM_FAILED){
            station_number[i] = sem_open(sem_name, 0);
            if (station_number[i]  == SEM_FAILED)
            {
                perror("Chyba při otevírání semaforu station_number");
                exit(1);
            }
        }
    }
}

shared_memory *createSharedMemory()
{
    int sharedMemoryId;
    shared_memory *sharedMemory;

    // Shared memory allocation
    sharedMemoryId = shmget(shm_id, sizeof(shared_memory), IPC_CREAT | 0666);
    if (sharedMemoryId == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Shared memory attachment
    sharedMemory = (shared_memory *)shmat(sharedMemoryId, NULL, 0);
    if (sharedMemory == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Memory initialization
    for (int i = 1; i <= Z; ++i)
    {
        sharedMemory->skiers_waiting[i] = 0;
    }
    sharedMemory->skiers_boarded = 0;
    sharedMemory->current_station = 0;
    sharedMemory->event_number = 1;

    return sharedMemory;
}

void bus_process(int stations, int max_bus_ride_time, shared_memory *shared_mem)
{
    sem_wait(&mutex);
    printf("%d: BUS: started\n", shared_mem->event_number);
    shared_mem->event_number++;
    sem_post(&mutex);
    int idZ = 1;

    while (idZ <= stations)
    {
        // Čekání na příjezd autobusu na zastávku
        usleep(rand() % max_bus_ride_time); // Čekání na náhodný čas dojezdu na další zastávku
        sem_wait(&mutex);
        printf("%d: BUS: arrived to %d\n", shared_mem->event_number, idZ);
        sem_post(station_number[idZ-1]);
        
        shared_mem->event_number++;
        shared_mem->current_station = idZ;
        sem_post(&mutex);
        sem_wait(&mutex);

        if(shared_mem->skiers_waiting[idZ] != 0){
            sem_wait(boardingS);
        }
        printf("%d: BUS: leaving %d\n", shared_mem->event_number, idZ);
        shared_mem->event_number++;

        sem_post(&mutex);
        // Nechá nastoupit všechny čekající lyžaře do kapacity autobusu

        // Pokud idZ < Z, tak idZ = idZ + 1 a pokračuje bodem (*)

        sem_wait(&mutex);
        if (shared_mem->current_station < stations)
            idZ++;
        else
        {
            printf("%d: BUS: arrived to final\n", shared_mem->event_number);
            shared_mem->event_number++;
            sem_post(bus_finish);
            sem_wait(leaving_bus);
            usleep(rand() % max_bus_ride_time); // Čekání na náhodný čas
            printf("%d: BUS: leaving final\n", shared_mem->event_number);
            shared_mem->event_number++;
            printf("%d: BUS: finish\n", shared_mem->event_number);
            shared_mem->event_number++;
            break;
        }
        sem_post(&mutex);
    }

    // Proces autobusu končí
}

void skier_process(int idL, int idZ, int max_waiting_time, shared_memory *shared_mem)
{
    // Spuštění lyžaře
    sem_wait(&mutex);
    printf("%d: L%d: started\n", shared_mem->event_number, idL);
    shared_mem->event_number++;
    sem_post(&mutex);
    // Dojí snídani---čeká v intervalu <0,TL> mikrosekund.
    usleep(rand() % max_waiting_time);
    // Jde na přidělenou zastávku idZ.
    sem_wait(&mutex);
    shared_mem->skiers_waiting[idZ]++;
    
    
    printf("%d: L%d: arrived to %d\n", shared_mem->event_number, idL, idZ);
    shared_mem->event_number++;
    // Po příjezdu skibusu nastoupí (pokud je volná kapacita)
    sem_post(&mutex);

    sem_wait(station_number[idZ-1]);
    sem_wait(&mutex);
    if(idZ == shared_mem->current_station){
        if(shared_mem->skiers_waiting[idZ] > 0){
            printf("%d: L%d: boarding\n", shared_mem->event_number, idL);
            shared_mem->event_number++;
            shared_mem->skiers_boarded++;
            shared_mem->skiers_waiting[idZ]--;
            sem_post(station_number[idZ-1]);
        }
        if(shared_mem->skiers_waiting[idZ] == 0){
            sem_post(station_number[idZ-1]);
            sem_post(boardingS);
        }
    }
    sem_post(&mutex);

    sem_wait(bus_finish);
    sem_wait(&mutex);
    if(shared_mem->skiers_boarded > 0){
        printf("%d: L%d: going to ski\n", shared_mem->event_number, idL);
        shared_mem->event_number++;
        shared_mem->skiers_boarded--;
        sem_post(bus_finish);
    }
    if(shared_mem->skiers_boarded == 0){
        sem_post(bus_finish);
        sem_post(leaving_bus);
    }
    sem_post(&mutex);
}

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Spatny pocet argumentu! Pouzijte: ./proj2 L Z K TL TB");
        return 1;
    }
    int L = atoi(argv[1]);  // Počet lyžařů
    Z = atoi(argv[2]);      // Počet zastávek
    int K = atoi(argv[3]);  // Kapacita autobusu
    int TL = atoi(argv[4]); // Maximální čekací doba lyžařů
    int TB = atoi(argv[5]); // Maximální doba jízdy autobusu
    if (L < 0 || L >= 20000 || Z < 1 || Z > 10 || K < 10 || K > 100 || TL < 0 || TL > 10000 || TB < 0 || TB > 1000)
    {
        printf("Neplatne hodnoty argumentu!\n");
        return 1;
    }

    initialize_semaphore();
    shared_memory *shared_mem = createSharedMemory();
    
    // Spuštění procesu autobusu
    pid_t bus_pid = fork();
    if (bus_pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (bus_pid == 0)
    {
        
        bus_process(Z, TB, shared_mem);
        exit(0);
    }

    for (int i = 1; i <= L; i++)
    {
        // Generování náhodné zastávky

        int random_station = rand() % Z + 1;

        pid_t skier_pid = fork();
        if (skier_pid == -1)
        {
            perror("fork");
            exit(1);
        }
        else if (skier_pid == 0)
        {
            // Proces lyžaře
            
            skier_process(i, random_station, TL, shared_mem);
            exit(0);
        }
    }

    // Čekání na ukončení procesu autobusu
    if (wait(NULL) == -1)
    {
        perror("wait");
        exit(1);
    }

    // Čekání na ukončení všech procesů lyžařů
    for (int i = 0; i < L; i++)
    {
        if (wait(NULL) == -1)
        {
            perror("wait");
            exit(1);
        }
    }
    // Odstranění semaforu
    for(int i = 0; i < 10; i++){
        sem_close(station_number[i]);
    }
    sem_close(boardingS);
    // Odstranění sdílené paměti
    
    return 0;
}