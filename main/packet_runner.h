#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PACKET_COLS 18
#define PACKET_ROWS 7
#define PACKET_LEVELS 3
#define PACKET_PATROLS 2
#define PACKET_CHASER 2
#define PACKET_ENEMIES 3
#define PACKET_SHIELDS 2
#define PACKET_RULESET 1
#define PACKET_ENEMY_PERIOD 8
// Entirely local game state: no radio, storage, clock or allocation dependencies.
typedef struct {
    unsigned level, moves, drops, route_moves, route_drops, attempt_moves;
    uint8_t x, y, relays, turn, shields_taken, chaser_x, chaser_y, completed_routes;
    bool delivered, bumped, dropped, intercepted, shield, shielded, picked_shield, advanced, retried, chaser_ready;
} packet_runner_t;
char packet_runner_tile(unsigned level, int x, int y);
unsigned packet_runner_relay_bit(unsigned level, int x, int y);
unsigned packet_runner_shield_bit(unsigned level,int x,int y);
void packet_runner_reset(packet_runner_t *game);
void packet_runner_retry(packet_runner_t *game);
void packet_runner_move(packet_runner_t *game, int dx, int dy);
bool packet_runner_enemy(const packet_runner_t *game, unsigned enemy, int *x, int *y);
void packet_runner_next(packet_runner_t *game);
bool packet_runner_secret(unsigned *progress, char key);
