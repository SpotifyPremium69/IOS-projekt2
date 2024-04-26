/**
 ***********************************************************************************
 * @file proj2.c
 * @author Matěj Křenek <xkrenem00@stud.fit.vutbr.cz>
 * @brief Multi process application for IOS course
 * @date 2024-24-04
 *
 * @copyright Copyright (c) 2024
 ***********************************************************************************
 */

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "proj2.h"
#include <asm-generic/fcntl.h>

Semaphore semaphore;

int main(int argc, char *argv[])
{
    Config config = arg_parse(argc, argv);
    SharedMemory memory = memory_init();
    FILE *output = output_open("proj2.out");

    semaphore = semaphore_init();

    pid_t skibus_pid = fork();

    if (skibus_pid < 0)
    {
        fprintf(stderr, "Skibus process fork failed\n");

        return 1;
    }
    else if (skibus_pid == 0)
    {

        process_skibus(&config, &memory, output);
    }
    else
    {

        for (int i = 0; i < config.L; i++)
        {
            pid_t skier_pid = fork();

            if (skier_pid < 0)
            {
                fprintf(stderr, "Skier process fork failed\n");

                return 1;
            }
            else if (skier_pid == 0)
            {
                process_skier(&config, &memory, output);
                break;
            }
        }
    }

    output_close(output);
    semaphore_clean(&semaphore);
    memory_clean(&memory);

    return 0;
}

SharedMemory memory_init()
{
    SharedMemory memory;

    // Create shared memory for counter
    int shm_counter_id = shmget(IPC_PRIVATE, sizeof(memory.counter), IPC_CREAT | 0666);

    if (shm_counter_id < 0)
    {
        fprintf(stderr, "Error: SHMGET for counter failed");
        exit(1);
    }

    // Attach counter shared memory segment to our data space
    memory.counter = shmat(shm_counter_id, NULL, 0);

    if (memory.counter == (int *)-1)
    {
        fprintf(stderr, "Error: SHMAT for counter failed");
        exit(1);
    }

    // Initialize counter
    *(memory.counter) = 0;

    return memory;
}

void memory_clean(SharedMemory *memory)
{
    shmdt(memory->counter);
}

Semaphore semaphore_init()
{
    Semaphore semaphore;

    // Initialize semaphore
    sem_t *mutex = sem_open("/mutex_sem", O_CREAT, 0666, 1);

    if (mutex == SEM_FAILED)
    {
        fprintf(stderr, "Error: SEM_OPEN for mutex failed");
        exit(1);
    }

    semaphore.mutex = mutex;

    return semaphore;
}

void semaphore_clean(Semaphore *semaphore)
{
    sem_close(semaphore->mutex);
}

void process_skibus(Config *config, SharedMemory *memory, FILE *output)
{
    output_write(output, "BUS:", "started", memory->counter);
    printf("Skibus process! Start %d %d\n", *(memory->counter), config->L);
}

void process_skier(Config *config, SharedMemory *memory, FILE *output)
{
    output_write(output, "L:", "started", memory->counter);
    printf("Skier process! Start %d %d\n", *(memory->counter), config->L);
}

/**
 * @brief Parse array of given arguments and validate them
 * @param argc count of arguments
 * @param argv pointer to array of arguments
 * @return Bool status (false: Error, true: Success)
 */
Config arg_parse(int argc, char **argv)
{
    Config config;

    // Check if the correct number of arguments is provided
    if (argc != 6)
    {
        fprintf(stderr, "Usage: %s L Z K TL TB\n", argv[0]);
        exit(1);
    }

    // Parse and validate each argument
    if (!arg_validate(argv[1], 1, 19999))
    {
        fprintf(stderr, "Error: Invalid value for L. It should be a positive integer less than 20000.\n");
        exit(1);
    }
    config.L = atoi(argv[1]);

    if (!arg_validate(argv[2], 1, 10))
    {
        fprintf(stderr, "Error: Invalid value for Z. It should be a positive integer between 1 and 10.\n");
        exit(1);
    }
    config.Z = atoi(argv[2]);

    if (!arg_validate(argv[3], 10, 100))
    {
        fprintf(stderr, "Error: Invalid value for K. It should be an integer between 10 and 100.\n");
        exit(1);
    }
    config.K = atoi(argv[3]);

    if (!arg_validate(argv[4], 0, 10000))
    {
        fprintf(stderr, "Error: Invalid value for TL. It should be an integer between 0 and 10000.\n");
        exit(1);
    }
    config.TL = atoi(argv[4]);

    if (!arg_validate(argv[5], 0, 1000))
    {
        fprintf(stderr, "Error: Invalid value for TB. It should be an integer between 0 and 1000.\n");
        exit(1);
    }
    config.TB = atoi(argv[5]);

    return config;
}

/**
 * @brief Validate that given argument value is number and satisfies given range
 * @param str pointer to arg string to be validated
 * @param min min value of arg
 * @param max max value of arg
 * @return Bool status (false: Error, true: Success)
 */
bool arg_validate(const char *str, int min, int max)
{
    char *end;
    long val = strtol(str, &end, 10);

    if (*end != '\0' || val < min || val > max)
    {
        return false;
    }

    return true;
}

/**
 * @brief Open file for writing actions of given process
 * @param filename path to file to be opened
 * @return Pointer to opened file (NULL if failed)
 */
FILE *output_open(const char *filename)
{
    FILE *file = fopen(filename, "w");

    if (file == NULL)
    {
        fprintf(stderr, "Error: Unable to open file %s for writing.\n", filename);
        exit(1);
    }

    return file;
}

/**
 * @brief Write into output file in given format
 * @param file pointer to file
 * @param label label of record
 * @param value value of record
 */
void output_write(FILE *file, const char *label, const char *value, int *count)
{
    sem_wait(semaphore.mutex);

    *(count) = *(count) + 1;

    fprintf(file, "%d: %s: %s\n", *(count), label, value);
    fflush(file);

    sem_post(semaphore.mutex);
}

/**
 * @brief Close opened output file
 * @param file pointer to file
 */
void output_close(FILE *file)
{
    fclose(file);
}