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

#define MAX_WAITING_TIME 10000
#define MAX_BUS_RIDE_TIME 1000
#define MAX_STATIONS 10
#define MAX_CAPACITY 100

#define SEM_KEY "/my_semaphore"
#define SHM_KEY 5678

int event_number = 1;

// Semafory
sem_t *mutex;        // Synchronizace výstupu
sem_t *bus_arrived;  // Notifikace příjezdu autobusu
sem_t *bus_departed; // Notifikace odjezdu autobusu

struct shared_memory
{
    // Zde můžete definovat sdílené proměnné potřebné pro synchronizaci
    int skiers_waiting[MAX_STATIONS]; // Počet lyžařů čekajících na každé zastávce
    int skiers_boarded;               // Počet lyžařů, kteří už nastoupili do autobusu
    // Další proměnné podle potřeby
};

void create_semaphores()
{
    // Pokud semafor již existuje, otevřeme ho
    sem_t *mutex = sem_open(SEM_KEY, O_CREAT | O_EXCL, 0644, 1);
    if (mutex == SEM_FAILED)
    {
        // Pokud se semafor již existuje, otevřeme ho
        mutex = sem_open(SEM_KEY, 0);
        if (mutex == SEM_FAILED)
        {
            perror("Chyba při otevírání semaforu mutex");
            exit(1);
        }
    }

    // Stejně tak otevřeme i ostatní semafory
    bus_arrived = sem_open("/bus_arrived", O_CREAT | O_EXCL, 0644, 0);
    if (bus_arrived == SEM_FAILED)
    {
        bus_arrived = sem_open("/bus_arrived", 0);
        if (bus_arrived == SEM_FAILED)
        {
            perror("Chyba při otevírání semaforu bus_arrived");
            exit(1);
        }
    }

    bus_departed = sem_open("/bus_departed", O_CREAT | O_EXCL, 0644, 0);
    if (bus_departed == SEM_FAILED)
    {
        bus_departed = sem_open("/bus_departed", 0);
        if (bus_departed == SEM_FAILED)
        {
            perror("Chyba při otevírání semaforu bus_departed");
            exit(1);
        }
    }
}

int create_shared_memory()
{
    int shmid;
    if ((shmid = shmget(SHM_KEY, sizeof(struct shared_memory), IPC_CREAT | 0666)) == -1)
    {
        perror("Chyba při vytváření sdílené paměti");
        exit(1);
    }
    return shmid;
}

void skier_process(int id, int stations, int max_waiting_time)
{
    // Simulace chování lyžaře
    srand(time(NULL) + id);                            // Inicializace generátoru pseudonáhodných čísel pro každého lyžaře
    printf("%d: L %d: started\n", event_number++, id); // Výpis do výstupu

    usleep(rand() % (max_waiting_time + 1));                          // Čekání na náhodný čas
    int station = rand() % stations + 1;                              // Náhodná zastávka, na kterou lyžař zamíří
    printf("%d: L %d: arrived to %d\n", event_number++, id, station); // Výpis do výstupu

    // Čekání na příjezd skibusu
    printf("%d: L %d: waiting for the ski bus\n", event_number++, id);
    sem_wait(mutex);
    printf("%d: L %d: boarding\n", event_number++, id);

    // Vyčká na příjezd skibusu k lanovce.
    printf("%d: L %d: going to ski\n", event_number++, id);
    sem_post(mutex);

    // Proces končí
}

void bus_process(int stations, int capacity, int max_bus_ride_time, struct shared_memory *shared_mem)
{
    // Simulace chování autobusu
    printf("%d: BUS: started\n", event_number++);

    for (int station = 1; station <= stations; station++)
    {
        printf("%d: BUS: arrived to %d\n", event_number++, station);

        // Nastoupí lyžaři, pokud je dostatečná kapacita
        int remaining_capacity = capacity - shared_mem->skiers_boarded;
        int skiers_boarding = remaining_capacity < shared_mem->skiers_waiting[station] ? remaining_capacity : shared_mem->skiers_waiting[station];
        shared_mem->skiers_waiting[station] -= skiers_boarding;
        shared_mem->skiers_boarded += skiers_boarding;
        printf("%d: BUS: boarding\n", event_number++);

        // Jízda autobusu na další zastávku
        usleep(rand() % (max_bus_ride_time + 1));

        // Výstup lyžařů
        printf("%d: BUS: leaving %d\n", event_number++, station);
    }

    // Dokončení jízdy a výstup na konečné zastávce
    printf("%d: BUS: arrived to final\n", event_number++);
    shared_mem->skiers_boarded = 0;
    printf("%d: BUS: leaving final\n", event_number++);
    printf("%d: BUS: finish\n", event_number++);

    // Uvolnění sdílené paměti
    if (shmdt(shared_mem) == -1)
    {
        perror("shmdt");
        exit(1);
    }

    // Ukončení procesu autobusu
    exit(0);
}

void main_process(int num_skiers, int num_stations, int bus_capacity, int max_waiting_time, int max_bus_ride_time, struct shared_memory *shared_mem)
{
    // Vytvoření procesu pro autobus
    pid_t bus_pid = fork();
    if (bus_pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (bus_pid == 0)
    {
        // Proces autobusu
        bus_process(num_stations, bus_capacity, max_bus_ride_time, shared_mem);
        exit(0);
    }

    // Vytvoření procesů pro lyžaře
    for (int i = 0; i < num_skiers; i++)
    {
        pid_t skier_pid = fork();
        if (skier_pid == -1)
        {
            perror("fork");
            exit(1);
        }
        else if (skier_pid == 0)
        {
            // Proces lyžaře
            skier_process(i + 1, num_stations, max_waiting_time);
            exit(0);
        }
    }

    // Čekání na ukončení všech procesů lyžařů
    for (int i = 0; i < num_skiers; i++)
    {
        if (wait(NULL) == -1)
        {
            perror("wait");
            exit(1);
        }
    }

    // Čekání na ukončení procesu autobusu
    if (wait(NULL) == -1)
    {
        perror("wait");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Spatny pocet argumentu! Pouzijte: ./proj2 L Z K TL TB");
        return 1;
    }
    int L = atoi(argv[1]);  // Počet lyžařů
    int Z = atoi(argv[2]);  // Počet zastávek
    int K = atoi(argv[3]);  // Kapacita autobusu
    int TL = atoi(argv[4]); // Maximální čekací doba lyžařů
    int TB = atoi(argv[5]); // Maximální doba jízdy autobusu
    if (L < 0 || L >= 20000 || Z < 1 || Z > 10 || K < 10 || K > 100 || TL < 0 || TL > 10000 || TB < 0 || TB > 1000)
    {
        printf("Neplatne hodnoty argumentu!\n");
        return 1;
    }

    create_semaphores();
    // Inicializace sdílené paměti
    key_t shm_key = ftok(".", 'x');
    int shm_id = shmget(shm_key, sizeof(struct shared_memory), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("shmget");
        exit(1);
    }
    struct shared_memory *shared_mem = (struct shared_memory *)shmat(shm_id, NULL, 0);
    if (shared_mem == (void *)-1)
    {
        perror("shmat");
        exit(1);
    }

    // Spuštění hlavního procesu
    main_process(L, Z, K, TL, TB, shared_mem);

    // Uvolnění sdílené paměti
    if (shmdt(shared_mem) == -1)
    {
        perror("shmdt");
    }
    if (shmctl(shm_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl");
    }

    return 0;
}