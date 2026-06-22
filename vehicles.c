#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include "simulation.h"

extern struct SharedState* shared_state;

// Returns true if the light is red for the given vehicle direction
static bool is_light_red_for_vehicle(struct IntersectionState* intersection, enum Direction dir) {
    switch (dir) {
        case DIR_NORTH: return intersection->n_light != LIGHT_GREEN;
        case DIR_SOUTH: return intersection->s_light != LIGHT_GREEN;
        case DIR_EAST:  return intersection->e_light != LIGHT_GREEN;
        case DIR_WEST:  return intersection->w_light != LIGHT_GREEN;
        default: return true;
    }
}

// Determines the final direction after a turn at an intersection
static enum Direction turn_direction(enum Direction current, enum Destination dest) {
    if (dest == DEST_STRAIGHT) return current;
    if (dest == DEST_LEFT) {
        switch (current) {
            case DIR_NORTH: return DIR_WEST; case DIR_SOUTH: return DIR_EAST;
            case DIR_EAST: return DIR_NORTH; case DIR_WEST: return DIR_SOUTH;
        }
    }
    if (dest == DEST_RIGHT) {
        switch (current) {
            case DIR_NORTH: return DIR_EAST; case DIR_SOUTH: return DIR_WEST;
            case DIR_EAST: return DIR_SOUTH; case DIR_WEST: return DIR_NORTH;
        }
    }
    return current;
}

// Scans for other vehicles ahead to maintain safe distance and avoid collisions
static float check_ahead(struct SharedState* ss, int my_id, float mx, float my, enum Direction dir) {
    float min_d = 9999.0;
    for (int j = 0; j < MAX_VEHICLES; j++) {
        if (j == my_id || !ss->vehicles[j].active || ss->vehicles[j].is_parked || ss->vehicles[j].is_turning) continue;
        struct VehicleState* o = &ss->vehicles[j];
        if (o->dir != dir) continue;

        if (ss->vehicles[my_id].priority == PRIORITY_HIGH && o->priority != PRIORITY_HIGH) continue;

        float dist = 0; bool same = false;
        float check_w = (ss->vehicles[my_id].priority == PRIORITY_HIGH) ? (LANE_WIDTH * 0.5) : LANE_WIDTH;
        if (dir == DIR_EAST || dir == DIR_WEST) {
            if (fabsf(my - o->y) < check_w) { same = true; dist = (dir == DIR_EAST) ? (o->x - mx) : (mx - o->x); }
        } else {
            if (fabsf(mx - o->x) < check_w) { same = true; dist = (dir == DIR_SOUTH) ? (o->y - my) : (my - o->y); }
        }
        if (same && dist > 0 && dist < min_d) min_d = dist;
    }
    return min_d;
}

// Checks if an emergency vehicle is approaching from behind to trigger pull-over behavior
static bool check_emergency_behind(struct SharedState* ss, int my_id, float mx, float my, enum Direction dir) {
    for (int j = 0; j < MAX_VEHICLES; j++) {
        if (j == my_id || !ss->vehicles[j].active || ss->vehicles[j].is_parked) continue;
        struct VehicleState* o = &ss->vehicles[j];
        if (o->priority != PRIORITY_HIGH) continue;
        if (o->dir != dir) continue;
        
        float dist = 0; bool same_lane = false;
        if (dir == DIR_EAST || dir == DIR_WEST) {
            if (fabsf(my - o->y) < LANE_WIDTH * 1.5) { same_lane = true; dist = (dir == DIR_EAST) ? (mx - o->x) : (o->x - mx); }
        } else {
            if (fabsf(mx - o->x) < LANE_WIDTH * 1.5) { same_lane = true; dist = (dir == DIR_SOUTH) ? (my - o->y) : (o->y - my); }
        }
        if (same_lane && dist > 0 && dist < 200.0) return true;
    }
    return false;
}

// Initializes the parametric Bezier curve points for turning maneuvers
static void setup_turn(struct VehicleState* me, int ix, int iy) {
    enum Direction new_dir = turn_direction(me->dir, me->destination);
    float p1x, p1y, p2x, p2y;

    me->turn_p0x = me->x;
    me->turn_p0y = me->y;

    switch (me->dir) {
        case DIR_EAST:
            if (new_dir == DIR_SOUTH) { p2x = ix + 45; p2y = iy + ROAD_WIDTH + 60; }
            else { p2x = ix + 75; p2y = iy - 60; }
            p1x = p2x; p1y = me->y; 
            break;
        case DIR_WEST:
            if (new_dir == DIR_NORTH) { p2x = ix + 75; p2y = iy - 60; }
            else { p2x = ix + 45; p2y = iy + ROAD_WIDTH + 60; }
            p1x = p2x; p1y = me->y;
            break;
        case DIR_SOUTH:
            if (new_dir == DIR_WEST) { p2x = ix - 60; p2y = LANE_WB_2; }
            else { p2x = ix + ROAD_WIDTH + 60; p2y = LANE_EB_1; }
            p1x = me->x; p1y = p2y;
            break;
        case DIR_NORTH:
            if (new_dir == DIR_EAST) { p2x = ix + ROAD_WIDTH + 60; p2y = LANE_EB_1; }
            else { p2x = ix - 60; p2y = LANE_WB_2; }
            p1x = me->x; p1y = p2y;
            break;
        default: p1x = me->x; p1y = me->y; p2x = me->x; p2y = me->y;
    }

    me->turn_p1x = p1x; me->turn_p1y = p1y;
    me->turn_p2x = p2x; me->turn_p2y = p2y;
    me->is_turning = true;
    me->turn_t = 0.0;
}

// Main thread function representing a single vehicle's lifecycle and logic
void* vehicle_thread(void* arg) {
    int id = *((int*)arg);
    struct VehicleState* me = &shared_state->vehicles[id];

    while (!shared_state->start_signal) { usleep(100000); if (!shared_state->simulation_running) return NULL; }

    me->id = id; me->active = true; me->is_parked = false; me->is_turning = false;
    me->is_pulling_over = false;

    pthread_mutex_lock(&shared_state->emergency_mutex);
    int r = rand() % 100;
    if (r < 15 && shared_state->emergency_spawn_allowed) {
        me->type = (rand() % 2) ? AMBULANCE : FIRETRUCK;
        shared_state->active_emergency_count++;
        if (shared_state->active_emergency_count >= 2) {
            shared_state->emergency_spawn_allowed = false;
        }
    } else if (r < 30) me->type = BUS; 
    else if (r < 60) me->type = CAR;
    else if (r < 80) me->type = BIKE; 
    else me->type = TRACTOR;

    if (me->type == AMBULANCE || me->type == FIRETRUCK) me->priority = PRIORITY_HIGH;
    else if (me->type == BUS) me->priority = PRIORITY_MEDIUM;
    else me->priority = PRIORITY_LOW;
    pthread_mutex_unlock(&shared_state->emergency_mutex);

    bool is_emergency = (me->priority == PRIORITY_HIGH);

    int entry = rand() % 4;
    switch (entry) {
        case 0: me->dir = DIR_EAST; me->x = is_emergency ? -400.0 : -60.0;
            if (is_emergency) me->y = LANE_EB_1;
            else if (me->type == TRACTOR) me->y = LANE_EB_2;
            else { me->y = (rand() % 2) ? LANE_EB_1 : LANE_EB_2; }
            break;
        case 1: me->dir = DIR_WEST; me->x = is_emergency ? WINDOW_WIDTH + 400.0 : WINDOW_WIDTH + 60.0;
            if (is_emergency) me->y = LANE_WB_2;
            else if (me->type == TRACTOR) me->y = LANE_WB_1;
            else { me->y = (rand() % 2) ? LANE_WB_1 : LANE_WB_2; }
            break;
        case 2: me->dir = DIR_SOUTH;
            { int ix = (rand()%2) ? F10_X : F11_X; me->x = ix + 15 + (rand()%2)*LANE_WIDTH; }
            me->y = is_emergency ? -400.0 : -60.0;
            break;
        case 3: me->dir = DIR_NORTH;
            { int ix = (rand()%2) ? F10_X : F11_X; me->x = ix + 75 + (rand()%2)*LANE_WIDTH; }
            me->y = is_emergency ? WINDOW_HEIGHT + 400.0 : WINDOW_HEIGHT + 60.0; break;
    }

    int d = rand() % 3;
    me->destination = (d == 0) ? DEST_STRAIGHT : (d == 1) ? DEST_LEFT : DEST_RIGHT;

    me->parking_target = 0;
    if (!is_emergency && (rand() % 10 < 8)) me->parking_target = (rand() % 2) ? 10 : 11;

    bool has_parked = false, has_turned = false;
    bool signaled_f10 = false, signaled_f11 = false;
    bool acquired_f10 = false, acquired_f11 = false, parking_reserved = false;

    float base_speed = is_emergency ? 4.0 : (me->type == BUS) ? 3.0 : (me->type == TRACTOR) ? 1.0 : 2.0;

    LOG_EVENT("VEHIC", "[\033[1;32mSPAWN\033[0m] %s %02d entered simulation (%s, Priority: %s)", type_str(me->type), id, dir_str(me->dir), pri_str(me->priority));

    while (shared_state->simulation_running) {
        if (me->is_parked) { usleep(33000); continue; }

        if (me->is_turning) {
            float dx01 = me->turn_p1x - me->turn_p0x, dy01 = me->turn_p1y - me->turn_p0y;
            float dx12 = me->turn_p2x - me->turn_p1x, dy12 = me->turn_p2y - me->turn_p1y;
            float curve_len = sqrtf(dx01*dx01+dy01*dy01) + sqrtf(dx12*dx12+dy12*dy12);
            if (curve_len < 1.0f) curve_len = 1.0f;
            me->turn_t += base_speed / curve_len;
            if (me->turn_t >= 1.0f) {
                me->turn_t = 1.0f;
                me->is_turning = false;
                me->dir = turn_direction(me->dir, me->destination);
                has_turned = true;
            }
            float t = me->turn_t, u = 1.0f - t;
            me->x = u*u*me->turn_p0x + 2*u*t*me->turn_p1x + t*t*me->turn_p2x;
            me->y = u*u*me->turn_p0y + 2*u*t*me->turn_p1y + t*t*me->turn_p2y;
            usleep(33000);
            continue;
        }

        float speed = base_speed;

        float ahead = check_ahead(shared_state, id, me->x, me->y, me->dir);
        if (ahead < MIN_FOLLOW_DIST) speed = 0.0;
        else if (ahead < MIN_FOLLOW_DIST * 1.5) speed *= 0.5;

        if (!is_emergency) {
            bool emg_behind = check_emergency_behind(shared_state, id, me->x, me->y, me->dir);
            if (emg_behind && !me->is_turning && !acquired_f10 && !acquired_f11) {
                if (!me->is_pulling_over) {
                    me->is_pulling_over = true;
                    me->orig_x = me->x; me->orig_y = me->y;
                }
                speed *= 0.5;
                if (me->dir == DIR_EAST) { if (me->y < me->orig_y + 45) me->y += 2.0; } 
                if (me->dir == DIR_WEST) { if (me->y > me->orig_y - 45) me->y -= 2.0; }
                if (me->dir == DIR_SOUTH) { if (me->x > me->orig_x - 45) me->x -= 2.0; }
                if (me->dir == DIR_NORTH) { if (me->x < me->orig_x + 45) me->x += 2.0; }
            } else if (me->is_pulling_over) {
                bool reached = true;
                if (me->dir == DIR_EAST || me->dir == DIR_WEST) {
                    if (fabsf(me->y - me->orig_y) > 0.5) {
                        me->y += (me->y < me->orig_y) ? 2.0 : -2.0;
                        reached = false;
                    }
                } else {
                    if (fabsf(me->x - me->orig_x) > 1.0) {
                        me->x += (me->x < me->orig_x) ? 2.0 : -2.0;
                        reached = false;
                    }
                }
                if (reached) me->is_pulling_over = false;
            }
        }

        float dx10 = (F10_X + ROAD_WIDTH/2) - me->x;
        float dy10 = (F10_Y + ROAD_WIDTH/2) - me->y;
        bool near_f10 = false;
        bool on_f10_ew = (fabsf(dy10) < ROAD_WIDTH/2 + 20);
        bool on_f10_ns = (fabsf(dx10) < ROAD_WIDTH/2 + 20);
        if (me->dir == DIR_EAST  && dx10 > 0 && dx10 < 120 && on_f10_ew) near_f10 = true;
        if (me->dir == DIR_WEST  && dx10 < 0 && dx10 > -120 && on_f10_ew) near_f10 = true;
        if (me->dir == DIR_SOUTH && dy10 > 0 && dy10 < 120 && on_f10_ns) near_f10 = true;
        if (me->dir == DIR_NORTH && dy10 < 0 && dy10 > -120 && on_f10_ns) near_f10 = true;

        if (is_emergency && !signaled_f10) {
            bool far_near_f10 = false;
            if (me->dir == DIR_EAST  && dx10 > 0 && dx10 < 450 && on_f10_ew) far_near_f10 = true;
            if (me->dir == DIR_WEST  && dx10 < 0 && dx10 > -450 && on_f10_ew) far_near_f10 = true;
            if (me->dir == DIR_SOUTH && dy10 > 0 && dy10 < 450 && on_f10_ns) far_near_f10 = true;
            if (me->dir == DIR_NORTH && dy10 < 0 && dy10 > -450 && on_f10_ns) far_near_f10 = true;

            if (far_near_f10) {
                shared_state->f10.emergency_dir = me->dir; shared_state->f10.emergency_approaching = true; signaled_f10 = true;
            }
        }

        float sx_eb = F10_X - 30, sx_wb = F10_X + ROAD_WIDTH + 30;
        float sy_sb = F10_Y - 30, sy_nb = F10_Y + ROAD_WIDTH + 30;
        if (!is_emergency && near_f10 && is_light_red_for_vehicle(&shared_state->f10, me->dir)) {
            if (me->dir == DIR_EAST && me->x > sx_eb - 60 && me->x < sx_eb + 5) speed = 0.0;
            if (me->dir == DIR_WEST && me->x < sx_wb + 60 && me->x > sx_wb - 5) speed = 0.0;
            if (me->dir == DIR_SOUTH && me->y > sy_sb - 60 && me->y < sy_sb + 5) speed = 0.0;
            if (me->dir == DIR_NORTH && me->y < sy_nb + 60 && me->y > sy_nb - 5) speed = 0.0;
        }

        if (!has_turned && me->destination != DEST_STRAIGHT && near_f10 && speed > 0) {
            bool close = false;
            if (me->dir == DIR_EAST && dx10 > 40 && dx10 < 80) close = true;
            if (me->dir == DIR_WEST && dx10 < -40 && dx10 > -80) close = true;
            if (me->dir == DIR_SOUTH && dy10 > 40 && dy10 < 80) close = true;
            if (me->dir == DIR_NORTH && dy10 < -40 && dy10 > -80) close = true;
            if (close) { setup_turn(me, F10_X, F10_Y); usleep(33000); continue; }
        }

        bool in_f10 = (fabsf(dx10) < ROAD_WIDTH/2+25 && fabsf(dy10) < ROAD_WIDTH/2+25);
        if (in_f10 && !acquired_f10) {
            LOG_EVENT("VEHIC", "[\033[1;33mWAIT\033[0m] %s %02d waiting at F10 (Priority: %s)", type_str(me->type), id, pri_str(me->priority));
            if (is_emergency) {
                shared_state->f10.high_priority_waiting++;
                sem_wait(&shared_state->f10_crossing);
                shared_state->f10.high_priority_waiting--;
            } else if (me->type == BUS) {
                shared_state->f10.med_priority_waiting++;
                while (shared_state->f10.high_priority_waiting > 0) usleep(10000);
                sem_wait(&shared_state->f10_crossing);
                shared_state->f10.med_priority_waiting--;
            } else {
                while (shared_state->f10.high_priority_waiting > 0 ||
                       shared_state->f10.med_priority_waiting > 0) usleep(10000);
                sem_wait(&shared_state->f10_crossing);
            }
            acquired_f10 = true;
            LOG_EVENT("VEHIC", "[\033[1;36mENTER\033[0m] %s %02d entered intersection F10", type_str(me->type), id);
        }
        if (!in_f10 && acquired_f10) { 
            sem_post(&shared_state->f10_crossing); 
            acquired_f10 = false; 
            LOG_EVENT("VEHIC", "[\033[1;35mEXIT\033[0m] %s %02d cleared intersection F10", type_str(me->type), id);
        }

        if (!has_parked && !parking_reserved && me->parking_target == 10 && near_f10 && !in_f10) {
            if (sem_trywait(&shared_state->f10_parking_queue) == 0) {
                LOG_EVENT("PARK ", "[\033[1;34mQUEUE\033[0m] %s %02d waiting for F10 parking spot", type_str(me->type), id);
                me->is_parked = true;
                shared_state->f10_waiting_count++;
                sem_wait(&shared_state->f10_parking_spots); 
                shared_state->f10_waiting_count--;
                sem_post(&shared_state->f10_parking_queue);
                parking_reserved = true; shared_state->f10_parked_count++;
                LOG_EVENT("PARK ", "[\033[1;32mPARKED\033[0m] %s %02d parked at F10", type_str(me->type), id);
                for (int s = 0; s < 100 && shared_state->simulation_running; s++) usleep(100000);
                shared_state->f10_parked_count--; sem_post(&shared_state->f10_parking_spots);
                LOG_EVENT("PARK ", "[\033[1;35mLEAVE\033[0m] %s %02d left F10 parking", type_str(me->type), id);
                me->is_parked = false; has_parked = true; parking_reserved = false; me->parking_target = 0;
            } else me->parking_target = 0;
        }

        float dx11 = (F11_X + ROAD_WIDTH/2) - me->x;
        float dy11 = (F11_Y + ROAD_WIDTH/2) - me->y;
        bool near_f11 = false;
        bool on_f11_ew = (fabsf(dy11) < ROAD_WIDTH/2 + 20);
        bool on_f11_ns = (fabsf(dx11) < ROAD_WIDTH/2 + 20);
        if (me->dir == DIR_EAST  && dx11 > 0 && dx11 < 120 && on_f11_ew) near_f11 = true;
        if (me->dir == DIR_WEST  && dx11 < 0 && dx11 > -120 && on_f11_ew) near_f11 = true;
        if (me->dir == DIR_SOUTH && dy11 > 0 && dy11 < 120 && on_f11_ns) near_f11 = true;
        if (me->dir == DIR_NORTH && dy11 < 0 && dy11 > -120 && on_f11_ns) near_f11 = true;

        if (is_emergency && !signaled_f11) {
            bool far_near_f11 = false;
            if (me->dir == DIR_EAST  && dx11 > 0 && dx11 < 450 && on_f11_ew) far_near_f11 = true;
            if (me->dir == DIR_WEST  && dx11 < 0 && dx11 > -450 && on_f11_ew) far_near_f11 = true;
            if (me->dir == DIR_SOUTH && dy11 > 0 && dy11 < 450 && on_f11_ns) far_near_f11 = true;
            if (me->dir == DIR_NORTH && dy11 < 0 && dy11 > -450 && on_f11_ns) far_near_f11 = true;

            if (far_near_f11) {
                shared_state->f11.emergency_dir = me->dir; shared_state->f11.emergency_approaching = true; signaled_f11 = true;
            }
        }

        float sx_eb11 = F11_X - 30, sx_wb11 = F11_X + ROAD_WIDTH + 30;
        float sy_sb11 = F11_Y - 30, sy_nb11 = F11_Y + ROAD_WIDTH + 30;
        if (!is_emergency && near_f11 && is_light_red_for_vehicle(&shared_state->f11, me->dir)) {
            if (me->dir == DIR_EAST && me->x > sx_eb11 - 60 && me->x < sx_eb11 + 5) speed = 0.0;
            if (me->dir == DIR_WEST && me->x < sx_wb11 + 60 && me->x > sx_wb11 - 5) speed = 0.0;
            if (me->dir == DIR_SOUTH && me->y > sy_sb11 - 60 && me->y < sy_sb11 + 5) speed = 0.0;
            if (me->dir == DIR_NORTH && me->y < sy_nb11 + 60 && me->y > sy_nb11 - 5) speed = 0.0;
        }

        if (!has_turned && me->destination != DEST_STRAIGHT && near_f11 && speed > 0) {
            bool close = false;
            if (me->dir == DIR_EAST && dx11 > 40 && dx11 < 80) close = true;
            if (me->dir == DIR_WEST && dx11 < -40 && dx11 > -80) close = true;
            if (me->dir == DIR_SOUTH && dy11 > 40 && dy11 < 80) close = true;
            if (me->dir == DIR_NORTH && dy11 < -40 && dy11 > -80) close = true;
            if (close) { setup_turn(me, F11_X, F11_Y); usleep(33000); continue; }
        }

        bool in_f11 = (fabsf(dx11) < ROAD_WIDTH/2+25 && fabsf(dy11) < ROAD_WIDTH/2+25);
        if (in_f11 && !acquired_f11) {
            LOG_EVENT("VEHIC", "[\033[1;33mWAIT\033[0m] %s %02d waiting at F11 (Priority: %s)", type_str(me->type), id, pri_str(me->priority));
            if (is_emergency) {
                shared_state->f11.high_priority_waiting++;
                sem_wait(&shared_state->f11_crossing);
                shared_state->f11.high_priority_waiting--;
            } else if (me->type == BUS) {
                shared_state->f11.med_priority_waiting++;
                while (shared_state->f11.high_priority_waiting > 0) usleep(10000);
                sem_wait(&shared_state->f11_crossing);
                shared_state->f11.med_priority_waiting--;
            } else {
                while (shared_state->f11.high_priority_waiting > 0 ||
                       shared_state->f11.med_priority_waiting > 0) usleep(10000);
                sem_wait(&shared_state->f11_crossing);
            }
            acquired_f11 = true;
            LOG_EVENT("VEHIC", "[\033[1;36mENTER\033[0m] %s %02d entered intersection F11", type_str(me->type), id);
        }
        if (!in_f11 && acquired_f11) { 
            sem_post(&shared_state->f11_crossing); 
            acquired_f11 = false; 
            LOG_EVENT("VEHIC", "[\033[1;35mEXIT\033[0m] %s %02d cleared intersection F11", type_str(me->type), id);
        }

        if (!has_parked && !parking_reserved && me->parking_target == 11 && near_f11 && !in_f11) {
            if (sem_trywait(&shared_state->f11_parking_queue) == 0) {
                LOG_EVENT("PARK ", "[\033[1;34mQUEUE\033[0m] %s %02d waiting for F11 parking spot", type_str(me->type), id);
                me->is_parked = true;
                shared_state->f11_waiting_count++;
                sem_wait(&shared_state->f11_parking_spots); 
                shared_state->f11_waiting_count--;
                sem_post(&shared_state->f11_parking_queue);
                parking_reserved = true; shared_state->f11_parked_count++;
                LOG_EVENT("PARK ", "[\033[1;32mPARKED\033[0m] %s %02d parked at F11", type_str(me->type), id);
                for (int s = 0; s < 100 && shared_state->simulation_running; s++) usleep(100000);
                shared_state->f11_parked_count--; sem_post(&shared_state->f11_parking_spots);
                LOG_EVENT("PARK ", "[\033[1;35mLEAVE\033[0m] %s %02d left F11 parking", type_str(me->type), id);
                me->is_parked = false; has_parked = true; parking_reserved = false; me->parking_target = 0;
            } else me->parking_target = 0;
        }

        switch (me->dir) {
            case DIR_EAST: me->x += speed; break; case DIR_WEST: me->x -= speed; break;
            case DIR_SOUTH: me->y += speed; break; case DIR_NORTH: me->y -= speed; break;
        }
        if (me->x > WINDOW_WIDTH+500 || me->x < -500 || me->y > WINDOW_HEIGHT+500 || me->y < -500) break;
        usleep(33000);
    }

    if (acquired_f10) sem_post(&shared_state->f10_crossing);
    if (acquired_f11) sem_post(&shared_state->f11_crossing);
    
    if (is_emergency) {
        pthread_mutex_lock(&shared_state->emergency_mutex);
        shared_state->active_emergency_count--;
        if (shared_state->active_emergency_count == 0) {
            shared_state->emergency_spawn_allowed = true;
        }
        pthread_mutex_unlock(&shared_state->emergency_mutex);
    }
    
    me->active = false;
    LOG_EVENT("VEHIC", "[\033[1;31mDESPAWN\033[0m] %s %02d left simulation", type_str(me->type), id);
    return NULL;
}
