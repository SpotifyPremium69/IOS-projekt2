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

    // Spuštění procesu autobusu
    pid_t bus_pid = fork();
    if (bus_pid == -1)
    {
        perror("fork");
        exit(1);
    }
    else if (bus_pid == 0)
    {
        srand(time(NULL) * getpid());
        sleep(2);
        printf("OK%d\n", getpid());
        exit(0);
    }

    for (int i = 1; i <= L; i++)
    {
        // Generování náhodné zastávky
        srand(time(NULL) * getpid());

        pid_t skier_pid = fork();
        if (skier_pid == -1)
        {
            perror("fork");
            exit(1);
        }
        else if (skier_pid == 0)
        {
            // Proces lyžaře
            printf("PICA%d\n", getpid());
            exit(0);
        }
    }

    // Čekání na ukončení všech procesů lyžařů
    for (int i = 0; i < L + 1; i++)
    {
        if (wait(NULL) == -1)
        {
            perror("wait");
            exit(1);
        }
    }

    // Odstranění semaforu

    // Odstranění sdílené paměti
    return 0;
}