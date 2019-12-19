#ifndef INFO_H
#define INFO_H

#include <stdio.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>

#define MAP_SIZE_X 52
#define MAP_SIZE_Y 25
#define FIELD_OF_VIEW 5
#define BAGS 10
#define START_TREASURE 10
#define MAX_BEASTS 10

enum player_type
{
    HUMAN = 0,
    BOT
};

struct player_t
{
    int x;
    int y;
    char map[FIELD_OF_VIEW][FIELD_OF_VIEW];
    int safe_coins;
    int carry_coins;
    int deaths;
    int in_game;
    enum player_type type;
    sem_t request_move;
    char move;
    int in_bushes;
};

struct req_id
{
    int id[4]; //If id == 0 - slot is free, id == 1 - slot is ready to take, id == 2 - slot is already taken
    sem_t request_position;
    sem_t player_response;
};

struct beast_t
{
    int x;
    int y;
    int in_game;
};

struct players_t
{
    struct player_t *player[4];
    struct req_id *p_id;
    struct beast_t beast[MAX_BEASTS];
    sem_t move_beast;
    sem_t beast_response;
    sem_t sync_map;
};

struct treasure_t
{
    int amount;
    int x;
    int y;
};

struct board_t
{
    char map[MAP_SIZE_Y][MAP_SIZE_X];
    int camp_x;
    int camp_y;
    struct players_t *pplayers;
    pthread_mutex_t add_coin;
    pthread_mutex_t resp_player;
    struct treasure_t bags[BAGS];
};

#endif
