# Concurrent Traffic Intersection & Parking System Simulator

A real-time operating-systems simulation in C that models traffic flow across two connected intersections (`F10`, `F11`) with integrated parking lots, vehicle priority scheduling, emergency preemption, inter-process communication, and SDL2 graphics.

The project demonstrates practical use of:
- **Processes** (`fork`) for distributed intersection control and rendering
- **Threads** (`pthread`) for concurrent vehicle lifecycles
- **Shared memory** (`mmap`) for synchronized global simulation state
- **Semaphores** (`sem_t`) for crossing capacity and parking control
- **IPC via pipes** for emergency coordination between intersections

---

## Simulation Overview

The simulator spawns up to 15 vehicles with randomized type, direction, and destination:
- Vehicle types: **Ambulance, Firetruck, Bus, Car, Bike, Tractor**
- Priorities:
  - **High**: Ambulance, Firetruck
  - **Medium**: Bus
  - **Low**: Car, Bike, Tractor

Each vehicle:
1. Enters from one of four map boundaries
2. Navigates intersections with traffic light compliance
3. Optionally turns (left/right via smooth Bézier turn paths)
4. Optionally parks at F10 or F11 (if non-emergency)
5. Exits the map and despawns

---

## Key Features

### 1) Dual-Intersection Control
- Two independent controllers (`controller_f10`, `controller_f11`) run in separate processes.
- Traffic lights cycle through directions with green/yellow/red transitions.
- Emergency signals can temporarily override normal light cycles.

### 2) Emergency Vehicle Preemption
- Emergency vehicles broadcast approach events.
- Local and remote intersections clear and favor the emergency direction.
- Non-emergency vehicles can pull over behaviorally when emergency vehicles approach from behind.

### 3) Priority-Based Crossing Access
- Intersections use counting semaphores (`f10_crossing`, `f11_crossing`) to limit simultaneous crossing.
- High-priority vehicles are served first, medium next, then low.
- Waiting queues are tracked in shared state for fairness and visualization.

### 4) Parking Lot Concurrency
- Both F10 and F11 have:
  - **10 parking spots** (`*_parking_spots` semaphore)
  - **Queue capacity of 5** (`*_parking_queue` semaphore)
- Vehicles wait, park for a fixed duration, and release spots safely.

### 5) Real-Time SDL Visualization
- Animated launch/start screen
- Rendered roads, intersections, lane markings, signals, and parking lots
- Live vehicle sprites by type and direction
- Parking occupancy + queue indicators
- Emergency alert banner when active

### 6) Structured Event Logging
- Timestamped logs show spawn, waiting, entering, exiting, parking, and despawn events.
- Colored labels make priority/emergency activity easier to inspect.

---

## Architecture

### Runtime Components
- **Main process (`main.c`)**
  - Initializes shared memory + synchronization primitives
  - Spawns controller processes and graphics process
  - Spawns/joins vehicle threads
  - Handles graceful shutdown (including `SIGINT`)
- **Intersection controllers (`controllers.c`)**
  - Manage light state machines
  - Listen/send emergency IPC messages via pipes
- **Vehicle engine (`vehicles.c`)**
  - Implements per-vehicle decision loop:
    - lane-following
    - collision spacing
    - red-light checks
    - crossing semaphore acquisition/release
    - emergency pull-over logic
    - parking lifecycle
- **Graphics renderer (`graphics.c`)**
  - SDL2/SDL_ttf window, frame loop, and HUD-like overlays

### Shared Data Model
Defined in `simulation.h`:
- `VehicleState`
- `IntersectionState`
- `SharedState` (global synchronized state + semaphores + control flags)

---

## Project Structure

```text
.
├── main.c          # Process/thread orchestration, lifecycle, cleanup
├── controllers.c   # F10/F11 traffic light + emergency IPC logic
├── vehicles.c      # Vehicle behavior, priorities, turning, parking, crossing
├── graphics.c      # SDL rendering, UI overlays, signal and vehicle drawing
├── simulation.h    # Shared structs, enums, constants, function declarations
├── Makefile        # Build rules and linker flags
└── README.md
```

---

## Build & Run

### 1) Install Dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y build-essential libsdl2-dev libsdl2-ttf-dev
```

### 2) Build

```bash
make
```

This generates:
- `traffic_simulation`

### 3) Run

```bash
./traffic_simulation
```

In the launch screen, press any key or click to start the simulation.

### 4) Clean

```bash
make clean
```

---

## Notes

- The simulation is real-time and non-deterministic (randomized vehicle generation and behavior).
- Stop safely with **Ctrl+C**; the program performs graceful semaphore/process cleanup.
- If build fails with missing SDL headers (e.g., `SDL2/SDL.h`), install the SDL2 development packages shown above.
