#ifndef SIMULATION_H
#define SIMULATION_H

#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <stdio.h>

#define LOG_EVENT(comp, fmt, ...) do { \
    struct timeval tv; \
    gettimeofday(&tv, NULL); \
    printf("[%02ld.%03ld] [%-5s] " fmt "\n", tv.tv_sec % 100, tv.tv_usec / 1000, comp, ##__VA_ARGS__); \
} while(0)

static inline const char* type_str(int t) {
    switch(t) {
        case 0: return "\033[1;31mAmbulance\033[0m"; case 1: return "\033[1;31mFiretruck\033[0m";
        case 2: return "\033[1;34mBus\033[0m";       case 3: return "Car";
        case 4: return "Bike";                       case 5: return "Tractor";
        default: return "Unknown";
    }
}

static inline const char* dir_str(int d) {
    switch(d) {
        case 0: return "Northbound"; case 1: return "Southbound";
        case 2: return "Eastbound"; case 3: return "Westbound";
        default: return "Unknown";
    }
}

static inline const char* pri_str(int p) {
    return (p == 0) ? "\033[1;31mHIGH\033[0m" : (p == 1) ? "\033[1;34mMED\033[0m" : "LOW";
}

#define MAX_VEHICLES 15
#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 800

#define F10_X 350
#define F10_Y 340

#define F11_X 850
#define F11_Y 340

#define ROAD_WIDTH 120
#define LANE_WIDTH 30

#define LANE_WB_1 (F10_Y + 15)
#define LANE_WB_2 (F10_Y + 45)
#define LANE_EB_1 (F10_Y + 75)
#define LANE_EB_2 (F10_Y + 105)

#define MIN_FOLLOW_DIST 55
#define VEHICLE_W 48
#define VEHICLE_H 28

enum VehicleType { AMBULANCE, FIRETRUCK, BUS, CAR, BIKE, TRACTOR };
enum Direction { DIR_NORTH, DIR_SOUTH, DIR_EAST, DIR_WEST };
enum LightState { LIGHT_RED, LIGHT_YELLOW, LIGHT_GREEN };
enum Destination { DEST_STRAIGHT, DEST_LEFT, DEST_RIGHT };
enum Priority { PRIORITY_HIGH, PRIORITY_MEDIUM, PRIORITY_LOW };

struct VehicleState {
    int id;
    enum VehicleType type;
    enum Priority priority;
    float x;
    float y;
    enum Direction dir;
    enum Destination destination;
    int parking_target;
    bool active;
    bool is_parked;
    bool is_turning;
    float turn_t;
    float turn_p0x, turn_p0y;
    float turn_p1x, turn_p1y;
    float turn_p2x, turn_p2y;
    bool is_pulling_over;
    float orig_x, orig_y;
};

struct IntersectionState {
    enum LightState n_light;
    enum LightState s_light;
    enum LightState e_light;
    enum LightState w_light;
    bool emergency_active;
    bool emergency_approaching;
    enum Direction emergency_dir;
    int high_priority_waiting;
    int med_priority_waiting;
};

struct SharedState {
    struct VehicleState vehicles[MAX_VEHICLES];
    struct IntersectionState f10;
    struct IntersectionState f11;
    sem_t f10_parking_spots;
    sem_t f10_parking_queue;
    sem_t f11_parking_spots;
    sem_t f11_parking_queue;
    int f10_parked_count;
    int f10_waiting_count;
    int f11_parked_count;
    int f11_waiting_count;
    sem_t f10_crossing;
    sem_t f11_crossing;
    bool simulation_running;
    bool start_signal;
    int active_emergency_count;
    bool emergency_spawn_allowed;
    pthread_mutex_t emergency_mutex;
};

void controller_f10(int pipe_read, int pipe_write, struct SharedState* state);
void controller_f11(int pipe_read, int pipe_write, struct SharedState* state);
void* vehicle_thread(void* arg);
void run_graphics(struct SharedState* state);

#endif
