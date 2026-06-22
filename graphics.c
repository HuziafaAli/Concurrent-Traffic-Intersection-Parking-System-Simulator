#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include "simulation.h"

// Renders text on the screen using the provided font and color
void draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// Maps a LightState enum value to RGB color components
static void light_color(enum LightState s, int* r, int* g, int* b) {
    if (s == LIGHT_GREEN)       { *r=0;   *g=255; *b=0;   }
    else if (s == LIGHT_YELLOW) { *r=255; *g=230; *b=0;   }
    else                        { *r=255; *g=30;  *b=30;  }
}

// Renders a compact traffic signal dot with a housing and active color
void draw_signal_dot(SDL_Renderer* renderer, int cx, int cy, enum LightState state) {
    SDL_Rect bg = { cx - 8, cy - 8, 16, 16 };
    SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
    SDL_RenderFillRect(renderer, &bg);
    int r, g, b;
    light_color(state, &r, &g, &b);
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_Rect lamp = { cx - 6, cy - 6, 12, 12 };
    SDL_RenderFillRect(renderer, &lamp);
    SDL_SetRenderDrawColor(renderer, r, g, b, 80);
    SDL_Rect glow = { cx - 9, cy - 9, 18, 18 };
    SDL_RenderDrawRect(renderer, &glow);
}

// Main graphics process function: handles window, rendering loop, and input
void run_graphics(struct SharedState* state) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return;
    }

    SDL_Window* window = SDL_CreateWindow(
        "OS Project 2026 - Traffic Simulation", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );

    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24);
    if (!font) font = TTF_OpenFont("/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf", 24);
    if (!font) font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 24);

    SDL_Event e;
    bool quit = false;

    Uint32 startTime = SDL_GetTicks();
    while (!quit && !state->start_signal && state->simulation_running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                state->simulation_running = false;
                quit = true;
            } else if (e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN) {
                state->start_signal = true;
            }
        }

        Uint32 ticks = SDL_GetTicks() - startTime;
        float time = ticks / 1000.0f;

        // 1. Deep space background
        SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
        SDL_RenderClear(renderer);

        // 2. Moving grid background
        SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
        int gridOffset = (int)(time * 20) % 40;
        for (int x = -gridOffset; x < WINDOW_WIDTH; x += 40)
            SDL_RenderDrawLine(renderer, x, 0, x, WINDOW_HEIGHT);
        for (int y = -gridOffset; y < WINDOW_HEIGHT; y += 40)
            SDL_RenderDrawLine(renderer, 0, y, WINDOW_WIDTH, y);

        // 3. Floating decorative particles
        for (int i = 0; i < 20; i++) {
            int px = (int)(WINDOW_WIDTH * (0.5f + 0.4f * cosf(time * 0.2f + i)));
            int py = (int)(WINDOW_HEIGHT * (0.5f + 0.4f * sinf(time * 0.3f + i * 1.5f)));
            SDL_SetRenderDrawColor(renderer, 100, 100, 255, 100);
            SDL_Rect r = { px, py, 4, 4 };
            SDL_RenderFillRect(renderer, &r);
        }

        if (font) {
            // 4. Glowing Title
            float pulse = (sinf(time * 3.0f) + 1.0f) * 0.5f;
            SDL_Color glowColor = { (Uint8)(100 + 100 * pulse), (Uint8)(150 + 100 * pulse), 255, 255 };
            SDL_Color titleColor = { 255, 255, 255, 255 };
            
            // Draw glow layer
            draw_text(renderer, font, "OS TRAFFIC SIMULATION 2026", WINDOW_WIDTH/2 - 202, WINDOW_HEIGHT/2 - 82, glowColor);
            draw_text(renderer, font, "OS TRAFFIC SIMULATION 2026", WINDOW_WIDTH/2 - 198, WINDOW_HEIGHT/2 - 78, glowColor);
            // Draw main text
            draw_text(renderer, font, "OS TRAFFIC SIMULATION 2026", WINDOW_WIDTH/2 - 200, WINDOW_HEIGHT/2 - 80, titleColor);

            // 5. Pulsing Subtext
            Uint8 alpha = (Uint8)(150 + 105 * sinf(time * 5.0f));
            SDL_Color pulseGreen = { 100, 255, 100, alpha };
            draw_text(renderer, font, "PRESS ANY KEY TO LAUNCH", WINDOW_WIDTH/2 - 160, WINDOW_HEIGHT/2 + 20, pulseGreen);
            
            // 6. Loading Bar decoration
            SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
            SDL_Rect barBg = { WINDOW_WIDTH/2 - 150, WINDOW_HEIGHT/2 + 70, 300, 10 };
            SDL_RenderFillRect(renderer, &barBg);
            
            float progress = fmodf(time * 0.5f, 1.0f);
            SDL_SetRenderDrawColor(renderer, 100, 200, 255, 255);
            SDL_Rect barProgress = { WINDOW_WIDTH/2 - 150, WINDOW_HEIGHT/2 + 70, (int)(300 * progress), 10 };
            SDL_RenderFillRect(renderer, &barProgress);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS for smoother animation
    }

    while (!quit && state->simulation_running) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                state->simulation_running = false;
                quit = true;
            }
        }

        SDL_SetRenderDrawColor(renderer, 76, 140, 60, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 195, 175, 145, 255);
        int curve_r = 80;
        int curve_w = 28;
        for (int corner = 0; corner < 8; corner++) {
            int ix, iy;
            float a_start;
            if (corner < 4) { ix = F10_X; iy = F10_Y; }
            else { ix = F11_X; iy = F11_Y; }
            int c = corner % 4;
            int cx, cy;
            switch (c) {
                case 0: cx = ix; cy = iy; a_start = 3.14159f; break;
                case 1: cx = ix + ROAD_WIDTH; cy = iy; a_start = 3.14159f * 1.5f; break;
                case 2: cx = ix + ROAD_WIDTH; cy = iy + ROAD_WIDTH; a_start = 0; break;
                default: cx = ix; cy = iy + ROAD_WIDTH; a_start = 3.14159f * 0.5f; break;
            }
            for (int t = 0; t <= 25; t++) {
                float a = a_start + (float)t / 25.0f * (3.14159f / 2.0f);
                int px = cx + (int)(curve_r * cosf(a));
                int py = cy + (int)(curve_r * sinf(a));
                SDL_Rect dot = { px - curve_w/2, py - curve_w/2, curve_w, curve_w };
                SDL_RenderFillRect(renderer, &dot);
            }
        }

        SDL_SetRenderDrawColor(renderer, 195, 175, 145, 255);
        SDL_Rect ew_road = { 0, F10_Y, WINDOW_WIDTH, ROAD_WIDTH };
        SDL_RenderFillRect(renderer, &ew_road);
        SDL_Rect f10_ns_road = { F10_X, 0, ROAD_WIDTH, WINDOW_HEIGHT };
        SDL_Rect f11_ns_road = { F11_X, 0, ROAD_WIDTH, WINDOW_HEIGHT };
        SDL_RenderFillRect(renderer, &f10_ns_road);
        SDL_RenderFillRect(renderer, &f11_ns_road);

        SDL_SetRenderDrawColor(renderer, 140, 125, 100, 255);
        SDL_Rect ew_top = { 0, F10_Y - 2, WINDOW_WIDTH, 2 };
        SDL_Rect ew_bot = { 0, F10_Y + ROAD_WIDTH, WINDOW_WIDTH, 2 };
        SDL_RenderFillRect(renderer, &ew_top);
        SDL_RenderFillRect(renderer, &ew_bot);
        SDL_Rect f10_l = { F10_X - 2, 0, 2, WINDOW_HEIGHT };
        SDL_Rect f10_r = { F10_X + ROAD_WIDTH, 0, 2, WINDOW_HEIGHT };
        SDL_RenderFillRect(renderer, &f10_l);
        SDL_RenderFillRect(renderer, &f10_r);
        SDL_Rect f11_l = { F11_X - 2, 0, 2, WINDOW_HEIGHT };
        SDL_Rect f11_r = { F11_X + ROAD_WIDTH, 0, 2, WINDOW_HEIGHT };
        SDL_RenderFillRect(renderer, &f11_l);
        SDL_RenderFillRect(renderer, &f11_r);

        SDL_SetRenderDrawColor(renderer, 55, 120, 45, 255);
        SDL_Rect park_top = { F10_X + ROAD_WIDTH + 10, F10_Y - 60, F11_X - F10_X - ROAD_WIDTH - 20, 50 };
        SDL_RenderFillRect(renderer, &park_top);
        SDL_Rect park_bot = { F10_X + ROAD_WIDTH + 10, F10_Y + ROAD_WIDTH + 10, F11_X - F10_X - ROAD_WIDTH - 20, 50 };
        SDL_RenderFillRect(renderer, &park_bot);
        SDL_SetRenderDrawColor(renderer, 40, 100, 35, 255);
        SDL_Rect pb1 = { park_top.x, park_top.y, park_top.w, 2 };
        SDL_Rect pb2 = { park_top.x, park_top.y + park_top.h - 2, park_top.w, 2 };
        SDL_Rect pb3 = { park_bot.x, park_bot.y, park_bot.w, 2 };
        SDL_Rect pb4 = { park_bot.x, park_bot.y + park_bot.h - 2, park_bot.w, 2 };
        SDL_RenderFillRect(renderer, &pb1);
        SDL_RenderFillRect(renderer, &pb2);
        SDL_RenderFillRect(renderer, &pb3);
        SDL_RenderFillRect(renderer, &pb4);

        SDL_SetRenderDrawColor(renderer, 240, 240, 230, 255);
        for (int i = 5; i < ROAD_WIDTH; i += 20) {
            SDL_Rect z1_f10 = { F10_X - 18, F10_Y + i, 14, 10 };
            SDL_Rect z2_f10 = { F10_X + ROAD_WIDTH + 4, F10_Y + i, 14, 10 };
            SDL_RenderFillRect(renderer, &z1_f10);
            SDL_RenderFillRect(renderer, &z2_f10);
            SDL_Rect z1_f11 = { F11_X - 18, F11_Y + i, 14, 10 };
            SDL_Rect z2_f11 = { F11_X + ROAD_WIDTH + 4, F11_Y + i, 14, 10 };
            SDL_RenderFillRect(renderer, &z1_f11);
            SDL_RenderFillRect(renderer, &z2_f11);
            SDL_Rect z3_f10 = { F10_X + i, F10_Y - 18, 10, 14 };
            SDL_Rect z4_f10 = { F10_X + i, F10_Y + ROAD_WIDTH + 4, 10, 14 };
            SDL_RenderFillRect(renderer, &z3_f10);
            SDL_RenderFillRect(renderer, &z4_f10);
            SDL_Rect z3_f11 = { F11_X + i, F11_Y - 18, 10, 14 };
            SDL_Rect z4_f11 = { F11_X + i, F11_Y + ROAD_WIDTH + 4, 10, 14 };
            SDL_RenderFillRect(renderer, &z3_f11);
            SDL_RenderFillRect(renderer, &z4_f11);
        }

        SDL_SetRenderDrawColor(renderer, 220, 180, 50, 255);
        SDL_Rect cl1 = { 0, F10_Y + ROAD_WIDTH/2 - 1, F10_X, 3 };
        SDL_RenderFillRect(renderer, &cl1);
        SDL_Rect cl2 = { F10_X + ROAD_WIDTH, F10_Y + ROAD_WIDTH/2 - 1, F11_X - F10_X - ROAD_WIDTH, 3 };
        SDL_RenderFillRect(renderer, &cl2);
        SDL_Rect cl3 = { F11_X + ROAD_WIDTH, F10_Y + ROAD_WIDTH/2 - 1, WINDOW_WIDTH - F11_X - ROAD_WIDTH, 3 };
        SDL_RenderFillRect(renderer, &cl3);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        for (int x = 0; x < WINDOW_WIDTH; x += 40) {
            bool in_f10 = (x + 20 > F10_X && x < F10_X + ROAD_WIDTH);
            bool in_f11 = (x + 20 > F11_X && x < F11_X + ROAD_WIDTH);
            if (in_f10 || in_f11) continue;
            SDL_Rect dash1 = { x, F10_Y + LANE_WIDTH, 20, 2 };
            SDL_RenderFillRect(renderer, &dash1);
            SDL_Rect dash2 = { x, F10_Y + LANE_WIDTH * 3, 20, 2 };
            SDL_RenderFillRect(renderer, &dash2);
        }

        SDL_SetRenderDrawColor(renderer, 220, 180, 50, 255);
        SDL_Rect f10c1 = { F10_X + ROAD_WIDTH/2 - 1, 0, 3, F10_Y };
        SDL_RenderFillRect(renderer, &f10c1);
        SDL_Rect f10c2 = { F10_X + ROAD_WIDTH/2 - 1, F10_Y + ROAD_WIDTH, 3, WINDOW_HEIGHT - F10_Y - ROAD_WIDTH };
        SDL_RenderFillRect(renderer, &f10c2);
        SDL_Rect f11c1 = { F11_X + ROAD_WIDTH/2 - 1, 0, 3, F11_Y };
        SDL_RenderFillRect(renderer, &f11c1);
        SDL_Rect f11c2 = { F11_X + ROAD_WIDTH/2 - 1, F11_Y + ROAD_WIDTH, 3, WINDOW_HEIGHT - F11_Y - ROAD_WIDTH };
        SDL_RenderFillRect(renderer, &f11c2);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        for (int y = 0; y < WINDOW_HEIGHT; y += 40) {
            bool in_ew = (y + 20 > F10_Y && y < F10_Y + ROAD_WIDTH);
            if (in_ew) continue;
            SDL_Rect nd1 = { F10_X + LANE_WIDTH, y, 2, 20 };
            SDL_RenderFillRect(renderer, &nd1);
            SDL_Rect nd2 = { F10_X + LANE_WIDTH * 3, y, 2, 20 };
            SDL_RenderFillRect(renderer, &nd2);
            SDL_Rect nd3 = { F11_X + LANE_WIDTH, y, 2, 20 };
            SDL_RenderFillRect(renderer, &nd3);
            SDL_Rect nd4 = { F11_X + LANE_WIDTH * 3, y, 2, 20 };
            SDL_RenderFillRect(renderer, &nd4);
        }

        SDL_Rect f10_parking = { F10_X - 180, F10_Y + ROAD_WIDTH + 30, 170, 140 };
        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_RenderFillRect(renderer, &f10_parking);
        SDL_SetRenderDrawColor(renderer, 100, 100, 110, 255);
        SDL_RenderDrawRect(renderer, &f10_parking);
        SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        SDL_Rect f10_entry = { f10_parking.x + f10_parking.w, f10_parking.y + 10, 20, 30 };
        SDL_RenderFillRect(renderer, &f10_entry);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawLine(renderer, f10_entry.x + 10, f10_entry.y + 25, f10_entry.x + 10, f10_entry.y + 5);
        SDL_RenderDrawLine(renderer, f10_entry.x + 10, f10_entry.y + 5, f10_entry.x + 5, f10_entry.y + 12);
        SDL_RenderDrawLine(renderer, f10_entry.x + 10, f10_entry.y + 5, f10_entry.x + 15, f10_entry.y + 12);

        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 5; col++) {
                int sx = f10_parking.x + 6 + col * 32;
                int sy = f10_parking.y + 8 + row * 56;
                SDL_SetRenderDrawColor(renderer, 55, 55, 65, 255);
                SDL_Rect spot = { sx, sy, 28, 48 };
                SDL_RenderFillRect(renderer, &spot);
                SDL_SetRenderDrawColor(renderer, 180, 180, 190, 255);
                SDL_RenderDrawLine(renderer, sx, sy, sx, sy + 48);
                SDL_RenderDrawLine(renderer, sx + 28, sy, sx + 28, sy + 48);
                SDL_RenderDrawLine(renderer, sx, sy + 48, sx + 28, sy + 48);
            }
        }
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        SDL_Rect f10_lane1 = { f10_parking.x + 2, f10_parking.y + 55, f10_parking.w - 4, 5 };
        SDL_RenderFillRect(renderer, &f10_lane1);
        SDL_SetRenderDrawColor(renderer, 50, 110, 40, 255);
        SDL_Rect f10_green1 = { f10_parking.x - 8, f10_parking.y, 6, f10_parking.h };
        SDL_Rect f10_green2 = { f10_parking.x, f10_parking.y + f10_parking.h + 2, f10_parking.w, 6 };
        SDL_RenderFillRect(renderer, &f10_green1);
        SDL_RenderFillRect(renderer, &f10_green2);

        SDL_Rect f11_parking = { F11_X + ROAD_WIDTH + 10, F11_Y + ROAD_WIDTH + 30, 170, 140 };
        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_RenderFillRect(renderer, &f11_parking);
        SDL_SetRenderDrawColor(renderer, 100, 100, 110, 255);
        SDL_RenderDrawRect(renderer, &f11_parking);
        SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        SDL_Rect f11_entry = { f11_parking.x - 20, f11_parking.y + 10, 20, 30 };
        SDL_RenderFillRect(renderer, &f11_entry);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawLine(renderer, f11_entry.x + 10, f11_entry.y + 25, f11_entry.x + 10, f11_entry.y + 5);
        SDL_RenderDrawLine(renderer, f11_entry.x + 10, f11_entry.y + 5, f11_entry.x + 5, f11_entry.y + 12);
        SDL_RenderDrawLine(renderer, f11_entry.x + 10, f11_entry.y + 5, f11_entry.x + 15, f11_entry.y + 12);

        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 5; col++) {
                int sx = f11_parking.x + 6 + col * 32;
                int sy = f11_parking.y + 8 + row * 56;
                SDL_SetRenderDrawColor(renderer, 55, 55, 65, 255);
                SDL_Rect spot = { sx, sy, 28, 48 };
                SDL_RenderFillRect(renderer, &spot);
                SDL_SetRenderDrawColor(renderer, 180, 180, 190, 255);
                SDL_RenderDrawLine(renderer, sx, sy, sx, sy + 48);
                SDL_RenderDrawLine(renderer, sx + 28, sy, sx + 28, sy + 48);
                SDL_RenderDrawLine(renderer, sx, sy + 48, sx + 28, sy + 48);
            }
        }
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        SDL_Rect f11_lane1 = { f11_parking.x + 2, f11_parking.y + 55, f11_parking.w - 4, 5 };
        SDL_RenderFillRect(renderer, &f11_lane1);
        SDL_SetRenderDrawColor(renderer, 50, 110, 40, 255);
        SDL_Rect f11_green1 = { f11_parking.x + f11_parking.w + 2, f11_parking.y, 6, f11_parking.h };
        SDL_Rect f11_green2 = { f11_parking.x, f11_parking.y + f11_parking.h + 2, f11_parking.w, 6 };
        SDL_RenderFillRect(renderer, &f11_green1);
        SDL_RenderFillRect(renderer, &f11_green2);

        if (font) {
            SDL_Color white = {255, 255, 255, 255};
            draw_text(renderer, font, "F10", F10_X + ROAD_WIDTH/2 - 15, F10_Y - 45, white);
            draw_text(renderer, font, "F11", F11_X + ROAD_WIDTH/2 - 15, F11_Y - 45, white);

            SDL_Color p_color = {80, 160, 255, 255};
            draw_text(renderer, font, "P", f10_parking.x + f10_parking.w/2 - 6, f10_parking.y - 28, p_color);
            draw_text(renderer, font, "P", f11_parking.x + f11_parking.w/2 - 6, f11_parking.y - 28, p_color);

            char f10_txt[32], f11_txt[32], f10_q_txt[32], f11_q_txt[32];
            sprintf(f10_txt, "Spots: %d/10", state->f10_parked_count);
            sprintf(f11_txt, "Spots: %d/10", state->f11_parked_count);
            sprintf(f10_q_txt, "Queue: %d/5", state->f10_waiting_count);
            sprintf(f11_q_txt, "Queue: %d/5", state->f11_waiting_count);

            SDL_Color f10_occ_color;
            if (state->f10_parked_count < 10) f10_occ_color = (SDL_Color){100, 255, 100, 255};
            else if (state->f10_parked_count < 19) f10_occ_color = (SDL_Color){255, 220, 50, 255};
            else f10_occ_color = (SDL_Color){255, 80, 80, 255};

            SDL_Color f11_occ_color;
            if (state->f11_parked_count < 10) f11_occ_color = (SDL_Color){100, 255, 100, 255};
            else if (state->f11_parked_count < 19) f11_occ_color = (SDL_Color){255, 220, 50, 255};
            else f11_occ_color = (SDL_Color){255, 80, 80, 255};

            draw_text(renderer, font, f10_txt, f10_parking.x + f10_parking.w/2 - 40, f10_parking.y + f10_parking.h + 10, f10_occ_color);
            draw_text(renderer, font, f11_txt, f11_parking.x + f11_parking.w/2 - 40, f11_parking.y + f11_parking.h + 10, f11_occ_color);

            SDL_Color q_color = {150, 150, 255, 255};
            draw_text(renderer, font, f10_q_txt, f10_parking.x + f10_parking.w/2 - 40, f10_parking.y + f10_parking.h + 35, q_color);
            draw_text(renderer, font, f11_q_txt, f11_parking.x + f11_parking.w/2 - 40, f11_parking.y + f11_parking.h + 35, q_color);

            for (int i = 0; i < 5; i++) {
                SDL_Rect q10 = { f10_parking.x + f10_parking.w + 5, f10_parking.y + 10 + i * 15, 10, 10 };
                if (i < state->f10_waiting_count) SDL_SetRenderDrawColor(renderer, 150, 150, 255, 255);
                else SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
                SDL_RenderFillRect(renderer, &q10);
                
                SDL_Rect q11 = { f11_parking.x - 15, f11_parking.y + 10 + i * 15, 10, 10 };
                if (i < state->f11_waiting_count) SDL_SetRenderDrawColor(renderer, 150, 150, 255, 255);
                else SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
                SDL_RenderFillRect(renderer, &q11);
            }

            if (state->f10.emergency_active || state->f11.emergency_active) {
                if ((SDL_GetTicks() / 300) % 2 == 0) {
                    SDL_Color red_warn = {255, 50, 50, 255};
                    draw_text(renderer, font, "EMERGENCY VEHICLE APPROACHING!", WINDOW_WIDTH/2 - 200, 50, red_warn);
                    draw_text(renderer, font, "CLEARING INTERSECTIONS...", WINDOW_WIDTH/2 - 160, 80, red_warn);
                }
            }
        }

        for (int i = 0; i < MAX_VEHICLES; i++) {
            struct VehicleState v = state->vehicles[i];
            if (!v.active) continue;

            int vx = (int)v.x;
            int vy = (int)v.y;
            bool parked_vertical = false;

            if (v.is_parked) {
                int slot = 0;
                for (int j = 0; j < i; j++) {
                    if (state->vehicles[j].active && state->vehicles[j].is_parked
                        && state->vehicles[j].parking_target == v.parking_target)
                        slot++;
                }
                if (slot > 9) slot = 9;
                int col = slot % 5;
                int row = slot / 5;
                SDL_Rect* pk = (v.parking_target == 11) ? &f11_parking : &f10_parking;
                vx = pk->x + 6 + col * 32 + 14;
                vy = pk->y + 8 + row * 56 + 24;
                parked_vertical = true;
            }

            int bw = VEHICLE_W, bh = VEHICLE_H;
            bool vertical = (v.dir == DIR_NORTH || v.dir == DIR_SOUTH);
            if (parked_vertical) vertical = true;
            if (v.is_turning) {
                float t = v.turn_t, u = 1.0f - t;
                float tx = 2*u*(v.turn_p1x - v.turn_p0x) + 2*t*(v.turn_p2x - v.turn_p1x);
                float ty = 2*u*(v.turn_p1y - v.turn_p0y) + 2*t*(v.turn_p2y - v.turn_p1y);
                vertical = (fabsf(ty) > fabsf(tx));
            }
            if (vertical) { bw = VEHICLE_H; bh = VEHICLE_W; }

            int cr, cg, cb;
            switch (v.type) {
                case AMBULANCE: cr=255; cg=255; cb=255; break;
                case FIRETRUCK: cr=220; cg=30;  cb=30;  break;
                case BUS:       cr=255; cg=190; cb=0;   break;
                case CAR:       cr=40;  cg=100; cb=220; break;
                case BIKE:      cr=220; cg=80;  cb=180; break;
                case TRACTOR:   cr=130; cg=90;  cb=40;  break;
                default:        cr=128; cg=128; cb=128; break;
            }

            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 80);
            SDL_Rect shadow = { vx - bw/2 + 2, vy - bh/2 + 2, bw, bh };
            SDL_RenderFillRect(renderer, &shadow);

            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
            if (vertical) {
                SDL_Rect w1={vx-bw/2-2, vy-bh/2+4, 4,6}, w2={vx+bw/2-2, vy-bh/2+4, 4,6};
                SDL_Rect w3={vx-bw/2-2, vy+bh/2-10, 4,6}, w4={vx+bw/2-2, vy+bh/2-10, 4,6};
                SDL_RenderFillRect(renderer,&w1); SDL_RenderFillRect(renderer,&w2);
                SDL_RenderFillRect(renderer,&w3); SDL_RenderFillRect(renderer,&w4);
            } else {
                SDL_Rect w1={vx-bw/2+4, vy-bh/2-2, 6,4}, w2={vx+bw/2-10, vy-bh/2-2, 6,4};
                SDL_Rect w3={vx-bw/2+4, vy+bh/2-2, 6,4}, w4={vx+bw/2-10, vy+bh/2-2, 6,4};
                SDL_RenderFillRect(renderer,&w1); SDL_RenderFillRect(renderer,&w2);
                SDL_RenderFillRect(renderer,&w3); SDL_RenderFillRect(renderer,&w4);
            }

            SDL_SetRenderDrawColor(renderer, cr, cg, cb, 255);
            SDL_Rect body = { vx - bw/2, vy - bh/2, bw, bh };
            SDL_RenderFillRect(renderer, &body);

            SDL_SetRenderDrawColor(renderer, cr/2, cg/2, cb/2, 255);
            SDL_RenderDrawRect(renderer, &body);

            SDL_SetRenderDrawColor(renderer, 160, 210, 255, 255);
            if (!v.is_parked) {
                SDL_Rect ws;
                if (v.dir == DIR_EAST || (v.is_turning && !vertical))
                    ws = (SDL_Rect){ vx + bw/2 - 10, vy - bh/2 + 3, 8, bh - 6 };
                else if (v.dir == DIR_WEST)
                    ws = (SDL_Rect){ vx - bw/2 + 2, vy - bh/2 + 3, 8, bh - 6 };
                else if (v.dir == DIR_SOUTH)
                    ws = (SDL_Rect){ vx - bw/2 + 3, vy + bh/2 - 10, bw - 6, 8 };
                else
                    ws = (SDL_Rect){ vx - bw/2 + 3, vy - bh/2 + 2, bw - 6, 8 };
                SDL_RenderFillRect(renderer, &ws);
            }

            if (v.type == AMBULANCE || v.type == FIRETRUCK) {
                int vx = (int)v.x, vy = (int)v.y;
                if ((SDL_GetTicks() / 200) % 2)
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                else
                    SDL_SetRenderDrawColor(renderer, 0, 80, 255, 255);
                SDL_Rect siren = { vx - 4, vy - 3, 8, 6 };
                SDL_RenderFillRect(renderer, &siren);
            }
        }

        #define DRAW_SIGNAL(IX, IY, LIGHT, SLX, SLY, SLW, SLH, DX, DY) { \
            int _r, _g, _b; light_color(LIGHT, &_r, &_g, &_b);           \
            SDL_SetRenderDrawColor(renderer, _r, _g, _b, 200);            \
            SDL_Rect _sl = { SLX, SLY, SLW, SLH };                       \
            SDL_RenderFillRect(renderer, &_sl);                            \
            draw_signal_dot(renderer, DX, DY, LIGHT);                      \
        }

        DRAW_SIGNAL(F10_X, F10_Y, state->f10.e_light,
            F10_X - 3, F10_Y, 4, ROAD_WIDTH/2,
            F10_X - 10, F10_Y - 10)
        DRAW_SIGNAL(F10_X, F10_Y, state->f10.w_light,
            F10_X + ROAD_WIDTH - 1, F10_Y + ROAD_WIDTH/2, 4, ROAD_WIDTH/2,
            F10_X + ROAD_WIDTH + 10, F10_Y + ROAD_WIDTH + 10)
        DRAW_SIGNAL(F10_X, F10_Y, state->f10.s_light,
            F10_X, F10_Y - 3, ROAD_WIDTH/2, 4,
            F10_X + ROAD_WIDTH + 10, F10_Y - 10)
        DRAW_SIGNAL(F10_X, F10_Y, state->f10.n_light,
            F10_X + ROAD_WIDTH/2, F10_Y + ROAD_WIDTH - 1, ROAD_WIDTH/2, 4,
            F10_X - 10, F10_Y + ROAD_WIDTH + 10)

        DRAW_SIGNAL(F11_X, F11_Y, state->f11.e_light,
            F11_X - 3, F11_Y, 4, ROAD_WIDTH/2,
            F11_X - 10, F11_Y - 10)
        DRAW_SIGNAL(F11_X, F11_Y, state->f11.w_light,
            F11_X + ROAD_WIDTH - 1, F11_Y + ROAD_WIDTH/2, 4, ROAD_WIDTH/2,
            F11_X + ROAD_WIDTH + 10, F11_Y + ROAD_WIDTH + 10)
        DRAW_SIGNAL(F11_X, F11_Y, state->f11.s_light,
            F11_X, F11_Y - 3, ROAD_WIDTH/2, 4,
            F11_X + ROAD_WIDTH + 10, F11_Y - 10)
        DRAW_SIGNAL(F11_X, F11_Y, state->f11.n_light,
            F11_X + ROAD_WIDTH/2, F11_Y + ROAD_WIDTH - 1, ROAD_WIDTH/2, 4,
            F11_X - 10, F11_Y + ROAD_WIDTH + 10)

        #undef DRAW_SIGNAL

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}
