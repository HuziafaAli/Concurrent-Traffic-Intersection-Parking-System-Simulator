#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include "simulation.h"

// Set lights to favor a given direction by making all other lights red
static void set_lights_for_direction(struct IntersectionState* intersection, enum Direction dir) {
    intersection->n_light = LIGHT_RED;
    intersection->s_light = LIGHT_RED;
    intersection->e_light = LIGHT_RED;
    intersection->w_light = LIGHT_RED;

    switch (dir) {
        case DIR_NORTH: intersection->n_light = LIGHT_GREEN; break;
        case DIR_SOUTH: intersection->s_light = LIGHT_GREEN; break;
        case DIR_EAST:  intersection->e_light = LIGHT_GREEN; break;
        case DIR_WEST:  intersection->w_light = LIGHT_GREEN; break;
    }
}

// Turn the current green light to yellow in the intersection
static void set_current_to_yellow(struct IntersectionState* intersection) {
    if (intersection->n_light == LIGHT_GREEN) intersection->n_light = LIGHT_YELLOW;
    else if (intersection->s_light == LIGHT_GREEN) intersection->s_light = LIGHT_YELLOW;
    else if (intersection->e_light == LIGHT_GREEN) intersection->e_light = LIGHT_YELLOW;
    else if (intersection->w_light == LIGHT_GREEN) intersection->w_light = LIGHT_YELLOW;
}

// Main logic for intersection F10 controller process
void controller_f10(int pipe_read, int pipe_write, struct SharedState* state) {
    int flags = fcntl(pipe_read, F_GETFL, 0);
    fcntl(pipe_read, F_SETFL, flags | O_NONBLOCK);

    int timer = 0;
    while (state->simulation_running) {
        if (!state->start_signal) { usleep(100000); continue; }

        if (state->f10.emergency_approaching) {
            state->f10.emergency_active = true;
            set_lights_for_direction(&state->f10, state->f10.emergency_dir);
            timer = -100;
            char msg[16]; snprintf(msg, sizeof(msg), "EMG:%d", state->f10.emergency_dir);
            write(pipe_write, msg, strlen(msg) + 1);
            LOG_EVENT("F10  ", "[\033[1;31mEMERGENCY\033[0m] Local %s Emergency! Clearing path", dir_str(state->f10.emergency_dir));
            state->f10.emergency_approaching = false;
        }

        char buffer[16];
        if (read(pipe_read, buffer, sizeof(buffer) - 1) > 0) {
            if (strncmp(buffer, "EMG:", 4) == 0) {
                int recv_dir = atoi(&buffer[4]);
                state->f10.emergency_active = true;
                set_lights_for_direction(&state->f10, recv_dir);
                timer = -100;
                LOG_EVENT("F10  ", "[\033[1;31mEMERGENCY\033[0m] IPC from F11: %s Emergency!", dir_str(recv_dir));
            }
        }

        if (!state->f10.emergency_active) {
            timer++;
            if (timer == 60) {
                set_current_to_yellow(&state->f10);
            } else if (timer > 80) {
                if (state->f10.n_light == LIGHT_YELLOW || state->f10.n_light == LIGHT_GREEN) set_lights_for_direction(&state->f10, DIR_SOUTH);
                else if (state->f10.s_light == LIGHT_YELLOW || state->f10.s_light == LIGHT_GREEN) set_lights_for_direction(&state->f10, DIR_EAST);
                else if (state->f10.e_light == LIGHT_YELLOW || state->f10.e_light == LIGHT_GREEN) set_lights_for_direction(&state->f10, DIR_WEST);
                else set_lights_for_direction(&state->f10, DIR_NORTH);
                timer = 0;
            }
        } else {
            timer++;
            if (timer > 0) { state->f10.emergency_active = false; timer = 81; }
        }
        usleep(100000);
    }
}

// Main logic for intersection F11 controller process
void controller_f11(int pipe_read, int pipe_write, struct SharedState* state) {
    int flags = fcntl(pipe_read, F_GETFL, 0);
    fcntl(pipe_read, F_SETFL, flags | O_NONBLOCK);

    int timer = 0;
    while (state->simulation_running) {
        if (!state->start_signal) { usleep(100000); continue; }

        if (state->f11.emergency_approaching) {
            state->f11.emergency_active = true;
            set_lights_for_direction(&state->f11, state->f11.emergency_dir);
            timer = -100;
            char msg[16]; snprintf(msg, sizeof(msg), "EMG:%d", state->f11.emergency_dir);
            write(pipe_write, msg, strlen(msg) + 1);
            LOG_EVENT("F11  ", "[\033[1;31mEMERGENCY\033[0m] Local %s Emergency! Clearing path", dir_str(state->f11.emergency_dir));
            state->f11.emergency_approaching = false;
        }

        char buffer[16];
        if (read(pipe_read, buffer, sizeof(buffer) - 1) > 0) {
            if (strncmp(buffer, "EMG:", 4) == 0) {
                int recv_dir = atoi(&buffer[4]);
                state->f11.emergency_active = true;
                set_lights_for_direction(&state->f11, recv_dir);
                timer = -100;
                LOG_EVENT("F11  ", "[\033[1;31mEMERGENCY\033[0m] IPC from F10: %s Emergency!", dir_str(recv_dir));
            }
        }

        if (!state->f11.emergency_active) {
            timer++;
            if (timer == 60) {
                set_current_to_yellow(&state->f11);
            } else if (timer > 80) {
                if (state->f11.n_light == LIGHT_YELLOW || state->f11.n_light == LIGHT_GREEN) set_lights_for_direction(&state->f11, DIR_SOUTH);
                else if (state->f11.s_light == LIGHT_YELLOW || state->f11.s_light == LIGHT_GREEN) set_lights_for_direction(&state->f11, DIR_EAST);
                else if (state->f11.e_light == LIGHT_YELLOW || state->f11.e_light == LIGHT_GREEN) set_lights_for_direction(&state->f11, DIR_WEST);
                else set_lights_for_direction(&state->f11, DIR_NORTH);
                timer = 0;
            }
        } else {
            timer++;
            if (timer > 0) { state->f11.emergency_active = false; timer = 81; }
        }
        usleep(100000);
    }
}
