#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "simulation.h"

struct SharedState* shared_state = NULL;
pid_t graphics_pid = -1;
pid_t f10_pid = -1;
pid_t f11_pid = -1;

// Signal handler to shut down the simulation gracefully on CTRL+C
void handle_sigint(int sig) {
    (void)sig;
    if (shared_state) {
        shared_state->simulation_running = false;
        // Wake up any threads blocked on semaphores
        for (int i = 0; i < MAX_VEHICLES + 10; i++) {
            sem_post(&shared_state->f10_crossing);
            sem_post(&shared_state->f11_crossing);
            sem_post(&shared_state->f10_parking_spots);
            sem_post(&shared_state->f11_parking_spots);
            sem_post(&shared_state->f10_parking_queue);
            sem_post(&shared_state->f11_parking_queue);
        }
    }
    printf("\n[MAIN] Caught SIGINT. Shutting down gracefully...\n");
    if (graphics_pid > 0) kill(graphics_pid, SIGTERM);
    if (f10_pid > 0) kill(f10_pid, SIGTERM);
    if (f11_pid > 0) kill(f11_pid, SIGTERM);
}

// Program entry point: initializes resources and manages the simulation lifecycle
int main() {
    signal(SIGINT, handle_sigint);

    shared_state = mmap(NULL, sizeof(struct SharedState), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_state == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    memset(shared_state, 0, sizeof(struct SharedState));
    shared_state->simulation_running = true;
    shared_state->start_signal = false;

    sem_init(&shared_state->f10_parking_spots, 1, 10);
    sem_init(&shared_state->f10_parking_queue, 1, 5);
    sem_init(&shared_state->f11_parking_spots, 1, 10);
    sem_init(&shared_state->f11_parking_queue, 1, 5);

    sem_init(&shared_state->f10_crossing, 1, 4);
    sem_init(&shared_state->f11_crossing, 1, 4);

    shared_state->f10.n_light = LIGHT_GREEN;
    shared_state->f10.s_light = LIGHT_RED;
    shared_state->f10.e_light = LIGHT_RED;
    shared_state->f10.w_light = LIGHT_RED;

    shared_state->f11.n_light = LIGHT_GREEN;
    shared_state->f11.s_light = LIGHT_RED;
    shared_state->f11.e_light = LIGHT_RED;
    shared_state->f11.w_light = LIGHT_RED;

    int pipe_f10_to_f11[2];
    int pipe_f11_to_f10[2];
    if (pipe(pipe_f10_to_f11) == -1 || pipe(pipe_f11_to_f10) == -1) {
        perror("pipe failed");
        exit(1);
    }

    graphics_pid = fork();
    if (graphics_pid == 0) {
        run_graphics(shared_state);
        exit(0);
    }

    f10_pid = fork();
    if (f10_pid == 0) {
        close(pipe_f10_to_f11[0]);
        close(pipe_f11_to_f10[1]);
        controller_f10(pipe_f11_to_f10[0], pipe_f10_to_f11[1], shared_state);
        exit(0);
    }

    f11_pid = fork();
    if (f11_pid == 0) {
        close(pipe_f11_to_f10[0]);
        close(pipe_f10_to_f11[1]);
        controller_f11(pipe_f10_to_f11[0], pipe_f11_to_f10[1], shared_state);
        exit(0);
    }

    close(pipe_f10_to_f11[0]);
    close(pipe_f10_to_f11[1]);
    close(pipe_f11_to_f10[0]);
    close(pipe_f11_to_f10[1]);

    shared_state->emergency_spawn_allowed = true;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_state->emergency_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    // 6. Continuous Vehicle Spawning
    pthread_t vehicles[MAX_VEHICLES];
    bool thread_active[MAX_VEHICLES] = {false};
    int vehicle_ids[MAX_VEHICLES];

    srand(time(NULL));
    int total_spawned = 0;
    while (shared_state->simulation_running) {
        if (!shared_state->start_signal) { usleep(100000); continue; }

        int slot = -1;
        bool still_active = false;
        for (int i = 0; i < MAX_VEHICLES; i++) {
            if (!shared_state->vehicles[i].active) {
                if (thread_active[i]) {
                    pthread_join(vehicles[i], NULL);
                    thread_active[i] = false;
                }
                if (slot == -1 && total_spawned < MAX_VEHICLES) {
                    slot = i;
                }
            } else {
                still_active = true;
            }
        }

        if (slot != -1) {
            vehicle_ids[slot] = slot;
            if (pthread_create(&vehicles[slot], NULL, vehicle_thread, &vehicle_ids[slot]) == 0) {
                thread_active[slot] = true;
                total_spawned++;
            }
        } else if (total_spawned >= MAX_VEHICLES && !still_active) {
            // All vehicles have been spawned and have finished
            break;
        }
        usleep(100000 + (rand() % 400000));
    }

    // 7. Cleanup
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (thread_active[i]) pthread_join(vehicles[i], NULL);
    }

    shared_state->simulation_running = false;

    waitpid(graphics_pid, NULL, 0);
    waitpid(f10_pid, NULL, 0);
    waitpid(f11_pid, NULL, 0);

    sem_destroy(&shared_state->f10_parking_spots);
    sem_destroy(&shared_state->f10_parking_queue);
    sem_destroy(&shared_state->f11_parking_spots);
    sem_destroy(&shared_state->f11_parking_queue);
    sem_destroy(&shared_state->f10_crossing);
    sem_destroy(&shared_state->f11_crossing);
    pthread_mutex_destroy(&shared_state->emergency_mutex);
    munmap(shared_state, sizeof(struct SharedState));

    printf("[MAIN] Simulation ended successfully.\n");
    return 0;
}
