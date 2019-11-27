#include <stdio.h>
#include <ncurses.h>
#include "info.h"

void new_map(char *filename, char tab[MAP_SIZE_Y][MAP_SIZE_X]); //Function to read map from txt
void *resp_player(void *players); //Function that check if someone wants to join and give start coordinates
void *take_input(void *something); //Function to resp new coins pressing c, t or T and quiting server by pressing q
void *move_player(void *players); //Function that moves players and send maps
void new_pos(struct board_t *board, int id); //Function to give new position to plyer with id (used when players die)
void *print_board(void *board); //Function to print server map
void add_coin(struct board_t *board, int count, char *type); //Function to add given count of coins
void *move_beast(void *board); //Function to handle beast movement;

int main()
{
    int players_data = shm_open("/players", O_CREAT | O_RDWR, 0600); //Shared mem

    struct board_t *pboard = (struct board_t *)malloc(sizeof(struct board_t));

    ftruncate(players_data, sizeof(struct players_t));
    struct players_t *players = (struct players_t *)mmap(NULL, sizeof(struct players_t), PROT_READ | PROT_WRITE, MAP_SHARED, players_data, 0);

    pboard -> pplayers = players;

    new_map("map.txt", pboard->map); //Load map from file
    for(int i = 0; i < 4; i++) //Set no players in game
    {
        pboard->pplayers->player[i].in_game = 0;
        pboard->pplayers->player[i].x = -1;
        sem_init(&pboard->pplayers->player[i].request_move, 1, 0);
    }

    for(int i = 0; i < BAGS; i++) //Set no treasure bags
    {
        pboard->bags[i].x = -1;
        pboard->bags[i].y = -1;
    }

    for(int i = 0; i < MAX_BEASTS; i++)
    {
        pboard->pplayers->beast[i].in_game = 0;
        pboard->pplayers->beast[i].x = -1;
        pboard->pplayers->beast[i].y = -1;
    }

    pboard->pplayers->beast[0].in_game = 1; //Set one beast on start

    int x = 0, y = 0;
    while(pboard->map[y][x] != ' ') //Randomize position of campsite
    {
        x = rand() % MAP_SIZE_X;
        y = rand() % MAP_SIZE_Y;
    }
    pboard->camp_x = x;
    pboard->camp_y = y;
    pboard->map[pboard->camp_y][pboard->camp_x] = 'A';

    add_coin(pboard, START_TREASURE, NULL); //Add 10 random treasures on map

    sem_init(&players->player_response, 1, 0); //Initialize semaphores
    sem_init(&players->request_position, 1, 0);
    sem_init(&players->sync_map, 1, 0);
    pthread_mutex_init(&pboard->add_coin, NULL);

    initscr();
    curs_set(FALSE);
    start_color();
    use_default_colors();
    clear();
    init_pair(1, COLOR_WHITE, COLOR_WHITE);
    init_pair(2, COLOR_BLACK, COLOR_BLACK);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);
    init_pair(4, COLOR_WHITE, COLOR_MAGENTA);
    init_pair(5, COLOR_BLACK, COLOR_YELLOW);
    init_pair(6, COLOR_YELLOW, COLOR_GREEN);
    init_pair(7, COLOR_WHITE, -1);
    init_pair(8, COLOR_RED, COLOR_WHITE);
    srand(time(NULL));

    pthread_t resp_thread, quit_thread; //Thread to check whether someone tries to join, if yes randomize his position
    pthread_create(&resp_thread, NULL, resp_player, pboard);
    pthread_create(&quit_thread, NULL, take_input, pboard);

    pthread_t players_thread; //Thread for movement and map refresh
    pthread_t print_map_thread; //Thread to print map
    pthread_t beasts_thread; //Thread for beats

    pthread_create(&players_thread, NULL, move_player, pboard);
    pthread_create(&print_map_thread, NULL, print_board, pboard);

    pthread_create(&beasts_thread, NULL, move_beast, pboard);
    pthread_join(print_map_thread, NULL);

    getchar();
    endwin();
    shm_unlink("/players");
    free(pboard);
    close(players_data);
    return 0;
}

void *print_board(void *board)
{
    struct board_t *pboard = (struct board_t *)board;
    int round_num = 0;
    while(1)
    {   
        round_num++;
        clear();
        for(int y = 0; y < MAP_SIZE_Y; y++)
        {
            for(int x = 0; x < MAP_SIZE_X; x++)
            {
                if(pboard->map[y][x] == '|') //Print walls
                {
                    attron(COLOR_PAIR(2));
                    mvprintw(y, x, ".");
                }
                if(pboard->map[y][x] == ' ') //Print empty spaces
                {
                    attron(COLOR_PAIR(1));
                    mvprintw(y, x, "O");
                }
                if(pboard->map[y][x] == '#') //Print bushes
                {
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "#");
                }
                if(pboard->map[y][x] == 'c' || pboard->map[y][x] == 't' || pboard->map[y][x] == 'T')
                {                           //Print treasures
                    attron(COLOR_PAIR(5));
                    mvprintw(y, x, "%c", pboard->map[y][x]);
                }
                if(pboard->map[y][x] == 'A') //Print camspite
                {
                    attron(COLOR_PAIR(6));
                    mvprintw(pboard->camp_y, pboard->camp_x, "A");
                }
                for(int i = 0; i < BAGS; i++) //Check if there are bags on map and print them
                {
                    if(x == pboard->bags[i].x && y == pboard->bags[i].y)
                    {
                        attron(COLOR_PAIR(5));
                        mvprintw(y, x, "D");
                    }
                }
                if(x == pboard->pplayers->player[0].x && y == pboard->pplayers->player[0].y ) //Print player 1
                {
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "1");
                }
                if(x == pboard->pplayers->player[1].x && y == pboard->pplayers->player[1].y ) //Print player 2
                {
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "2");
                }
                if(x == pboard->pplayers->player[2].x && y == pboard->pplayers->player[2].y ) //Print player 3
                {
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "3");
                }
                if(x == pboard->pplayers->player[3].x && y == pboard->pplayers->player[3].y ) //Print player 4
                {
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "4");
                }
                for(int i = 0; i < MAX_BEASTS; i++) //Print beasts
                {
                    if(x == pboard->pplayers->beast[i].x && y == pboard->pplayers->beast[i].y)
                    {
                        attron(COLOR_PAIR(8));
                        mvprintw(y, x, "*");
                    }
                }
                
                attron(COLOR_PAIR(7)); mvprintw(2, 54, "Campsite X/Y: %d %d", pboard->camp_x, pboard->camp_y);
                mvprintw(3, 54, "Round number: %d", round_num);
                mvprintw(5, 54, "Parameter:  Player1 Player2 Player3 Player4");
                mvprintw(6, 54, "Type");
                mvprintw(7, 54, "Curr X/Y:");
                mvprintw(8, 54, "Deaths:");
                mvprintw(9, 54, "Coins");
                mvprintw(10, 56, "Carried");
                mvprintw(11, 56, "Brought");
                for(int i = 0; i < 4; i++)
                {
                    if(pboard->pplayers->player[i].in_game) //Print all info about players in game
                    {
                        if(i == 0) {if(pboard->pplayers->player[i].type==HUMAN)mvprintw(6, 66, "HUMAN"); else mvprintw(6, 66, "BOT"); mvprintw(7, 66, "%d/%d", pboard->pplayers->player[i].x, pboard->pplayers->player[i].y);mvprintw(8, 66, "%d", pboard->pplayers->player[i].deaths); mvprintw(10, 66, "%d", pboard->pplayers->player[i].carry_coins); mvprintw(11, 66, "%d", pboard->pplayers->player[i].safe_coins);}
                        if(i == 1) {if(pboard->pplayers->player[i].type==HUMAN)mvprintw(6, 74, "HUMAN"); else mvprintw(6, 74, "BOT"); mvprintw(7, 74, "%d/%d", pboard->pplayers->player[i].x, pboard->pplayers->player[i].y);mvprintw(8, 74, "%d", pboard->pplayers->player[i].deaths); mvprintw(10, 74, "%d", pboard->pplayers->player[i].carry_coins); mvprintw(11, 74, "%d", pboard->pplayers->player[i].safe_coins);}
                        if(i == 2) {if(pboard->pplayers->player[i].type==HUMAN)mvprintw(6, 82, "HUMAN"); else mvprintw(6, 82, "BOT"); mvprintw(7, 82, "%d/%d", pboard->pplayers->player[i].x, pboard->pplayers->player[i].y);mvprintw(8, 82, "%d", pboard->pplayers->player[i].deaths); mvprintw(10, 82, "%d", pboard->pplayers->player[i].carry_coins); mvprintw(11, 82, "%d", pboard->pplayers->player[i].safe_coins);}
                        if(i == 3) {if(pboard->pplayers->player[i].type==HUMAN)mvprintw(6, 90, "HUMAN"); else mvprintw(6, 90, "BOT"); mvprintw(7, 90, "%d/%d", pboard->pplayers->player[i].x, pboard->pplayers->player[i].y);mvprintw(8, 90, "%d", pboard->pplayers->player[i].deaths); mvprintw(10, 90, "%d", pboard->pplayers->player[i].carry_coins); mvprintw(11, 90, "%d", pboard->pplayers->player[i].safe_coins);}
                    }
                    else //Print - if players are not in game
                    {
                        if(i == 0) {mvprintw(6, 66, "-"); mvprintw(7, 66, "-"); mvprintw(8, 66, "-"); mvprintw(10, 66, "-"); mvprintw(11, 66, "-");}
                        if(i == 1) {mvprintw(6, 74, "-"); mvprintw(7, 74, "-"); mvprintw(8, 74, "-"); mvprintw(10, 74, "-"); mvprintw(11, 74, "-");}
                        if(i == 2) {mvprintw(6, 82, "-"); mvprintw(7, 82, "-"); mvprintw(8, 82, "-"); mvprintw(10, 82, "-"); mvprintw(11, 82, "-");}
                        if(i == 3) {mvprintw(6, 90, "-"); mvprintw(7, 90, "-"); mvprintw(8, 90, "-"); mvprintw(10, 90, "-"); mvprintw(11, 90, "-");}
                    }
                }
                mvprintw(14, 54, "Legend:");
                attron(COLOR_PAIR(4)); mvprintw(15, 55, "1234");
                attron(COLOR_PAIR(7)); mvprintw(15, 69, " - players");
                attron(COLOR_PAIR(2)); mvprintw(16, 55, "|");
                attron(COLOR_PAIR(7)); mvprintw(16, 69, " - wall");
                mvprintw(17, 55, "#"); mvprintw(17, 69, " - bushes");
                attron(COLOR_PAIR(8)); mvprintw(17, 55, "*"); 
                attron(COLOR_PAIR(7)); mvprintw(17, 69, " - wild beast");
                attron(COLOR_PAIR(5)); mvprintw(18, 55, "c");
                attron(COLOR_PAIR(7)); mvprintw(18, 69, " - one coin");
                attron(COLOR_PAIR(5)); mvprintw(19, 55, "t");
                attron(COLOR_PAIR(7)); mvprintw(19, 69, " - treasure (10 coins)");
                attron(COLOR_PAIR(5)); mvprintw(20, 55, "T");
                attron(COLOR_PAIR(7)); mvprintw(20, 69, " - large treasure (50 coins)");
                attron(COLOR_PAIR(5)); mvprintw(21, 55, "D");
                attron(COLOR_PAIR(7)); mvprintw(21, 69, " - dropped treasure");
                attron(COLOR_PAIR(6)); mvprintw(22, 55, "A");
                attron(COLOR_PAIR(7)); mvprintw(22, 69, " - campsite");         
            }
        }
        refresh();
        sem_wait(&pboard->pplayers->sync_map);
    }
    return NULL;
}

void *move_beast(void *board)
{
    struct board_t *pboard = (struct board_t *)board;

    while(1)
    {
        for(int i = 0; i < MAX_BEASTS; i++)
        {
            if(pboard->pplayers->beast[i].in_game)
            {
                int x, y;
                if(pboard->pplayers->beast[i].x == -1 || pboard->pplayers->beast[i].y == -1)
                {
                    x = 0, y = 0;
                    while(pboard->map[y][x] != ' ')
                    {
                        x = rand () % MAP_SIZE_X;
                        y = rand () % MAP_SIZE_Y;
                    }
                    pboard->pplayers->beast[i].x = x;
                    pboard->pplayers->beast[i].y = y;
                }
                int moved = 0;
                for(int j = 0; j < 4; j++)
                {
                    if(pboard->pplayers->player[j].in_game) //Check if player is in game
                    {
                        if(pboard->pplayers->player[j].x == pboard->pplayers->beast[i].x) //Check if they are in the same x 
                        {
                            x = pboard->pplayers->player[j].x;
                            int beast_y = pboard->pplayers->beast[i].y;
                            int player_y = pboard->pplayers->player[j].y;
                            int can_move = 1;
                            if(beast_y > player_y) //Beast is under player
                            {
                                for(; beast_y > player_y; beast_y--)
                                    if(pboard->map[beast_y][x] == '|') {can_move = 0; break;}
                                if(can_move) 
                                {
                                    pboard->pplayers->beast[i].y--;
                                    moved = 1;
                                }
                            }
                            else if(beast_y < player_y) //Beast is above player
                            {
                                for(; beast_y < player_y; beast_y++)
                                    if(pboard->map[beast_y][x] == '|') {can_move = 0; break;}
                                if(can_move) 
                                {
                                    pboard->pplayers->beast[i].y++;
                                    moved = 1;
                                }
                            }
                        }
                        else if(pboard->pplayers->player[j].y == pboard->pplayers->beast[i].y) //Check if they are in the same y 
                        {
                            y = pboard->pplayers->player[j].y;
                            int beast_x = pboard->pplayers->beast[i].x;
                            int player_x = pboard->pplayers->player[j].x;
                            int can_move = 1;
                            if(beast_x > player_x) //Beast is under player
                            {
                                for(; beast_x > player_x; beast_x--)
                                    if(pboard->map[y][beast_x] == '|') {can_move = 0; break;}
                                if(can_move) 
                                {
                                    pboard->pplayers->beast[i].x--;
                                    moved = 1;
                                }
                            }
                            else if(beast_x < player_x) //Beast is above player
                            {
                                for(; beast_x < player_x; beast_x++)
                                    if(pboard->map[y][beast_x] == '|') {can_move = 0; break;}
                                if(can_move) 
                                {
                                    pboard->pplayers->beast[i].x++;
                                    moved = 1;
                                }
                            }
                        }
                        if(moved) break; //Check if beast already moved
                    }
                }

                if(!moved) //No players detected, move random
                {
                    int random = rand() % 5;
                    char move = 'x';
                    if(random == 0) move = 'a';
                    if(random == 1) move = 's';
                    if(random == 2) move = 'd';
                    if(random == 3) move = 'w';
                    if(random == 4) move = 'x';

                    x = pboard->pplayers->beast[i].x;
                    y = pboard->pplayers->beast[i].y;

                    if(move == 'w') //Beast wants to move upwards
                    {
                        if(pboard->map[y-1][x] == ' ' || pboard->map[y-1][x] == 'c' || pboard->map[y-1][x] == 't' || pboard->map[y-1][x] == 'T' || pboard->map[y-1][x] == '#' || pboard->map[y-1][x] == 'A' || pboard->map[y-1][x] == 'D')
                        {
                            pboard->pplayers->beast[i].y--;
                        }
                    }
                    else if(move == 's') //Beast wants to move down
                    {
                        if(pboard->map[y+1][x] == ' ' || pboard->map[y+1][x] == 'c' || pboard->map[y+1][x] == 't' || pboard->map[y+1][x] == 'T' || pboard->map[y+1][x] == '#' || pboard->map[y+1][x] == 'A' || pboard->map[y+1][x] == 'D')
                        {
                            pboard->pplayers->beast[i].y++;
                        }
                    }
                    else if(move == 'a') //Beast wants to move left
                    {
                        if(pboard->map[y][x-1] == ' ' || pboard->map[y][x-1] == 'c' || pboard->map[y][x-1] == 't' || pboard->map[y][x-1] == 'T' || pboard->map[y][x-1] == '#' || pboard->map[y][x-1] == 'A' || pboard->map[y][x-1] == 'D')
                        {
                            pboard->pplayers->beast[i].x--;
                        }
                    }
                    else if(move == 'd') //Beast wants to move right
                    {
                        if(pboard->map[y][x+1] == ' ' || pboard->map[y][x+1] == 'c' || pboard->map[y][x+1] == 't' || pboard->map[y][x+1] == 'T' || pboard->map[y][x+1] == '#' || pboard->map[y][x+1] == 'A' || pboard->map[y][x+1] == 'D')
                        {
                            pboard->pplayers->beast[i].x++;
                        }
                    }
                }
            }
        }
        usleep(1000000);
    }

    return NULL;
}

void *move_player(void *players)
{
    struct board_t *pboard = (struct board_t *)players;

    while(1)
    {
        for(int i = 0; i < 4; i++)
        {
            if(pboard->pplayers->player[i].in_game && pboard->pplayers->player[i].x > 0)
            {
                int p_x = pboard->pplayers->player[i].x;
                int p_y = pboard->pplayers->player[i].y;
                int moved = 0;
                //Check if player can make a move
                if(pboard->pplayers->player[i].move == 'A' || pboard->pplayers->player[i].move == 'w') //Move up
                    if(pboard->map[p_y-1][p_x] == ' ' || pboard->map[p_y-1][p_x] == 'c' || pboard->map[p_y-1][p_x] == 't' || pboard->map[p_y-1][p_x] == 'T' || pboard->map[p_y-1][p_x] == '#' || pboard->map[p_y-1][p_x] == 'A' || pboard->map[p_y-1][p_x] == 'D')  
                        {
                            if(pboard->pplayers->player[i].in_bushes) pboard->pplayers->player[i].in_bushes = 0; //If player is in bush wait...
                            else
                            {
                                pboard->pplayers->player[i].y--; moved=1;
                                if(pboard->map[p_y-1][p_x] == 'c') {pboard->pplayers->player[i].carry_coins++; pboard->map[p_y-1][p_x] = ' ';}
                                if(pboard->map[p_y-1][p_x] == 't') {pboard->pplayers->player[i].carry_coins+=10; pboard->map[p_y-1][p_x] = ' ';}
                                if(pboard->map[p_y-1][p_x] == 'T') {pboard->pplayers->player[i].carry_coins+=50; pboard->map[p_y-1][p_x] = ' ';}
                                if(pboard->map[p_y-1][p_x] == 'A') {pboard->pplayers->player[i].safe_coins += pboard->pplayers->player[i].carry_coins; pboard->pplayers->player[i].carry_coins = 0;}
                                if(pboard->map[p_y-1][p_x] == '#')
                                    pboard->pplayers->player[i].in_bushes = 1;
                                for(int n = 0; n < BAGS; n++)
                                {
                                    if(p_x == pboard->bags[n].x && p_y-1 == pboard->bags[n].y)
                                    {
                                        pboard->pplayers->player[i].carry_coins += pboard->bags[n].amount;
                                        pboard->bags[n].x = -1;
                                        pboard->bags[n].y = -1;
                                        break;
                                    }
                                }
                            }
                        }
                if(pboard->pplayers->player[i].move == 'B' || pboard->pplayers->player[i].move == 's') //Move down
                    if(pboard->map[p_y+1][p_x] == ' ' || pboard->map[p_y+1][p_x] == 'c' || pboard->map[p_y+1][p_x] == 't' || pboard->map[p_y+1][p_x] == 'T' || pboard->map[p_y+1][p_x] == '#' || pboard->map[p_y+1][p_x] == 'A' || pboard->map[p_y+1][p_x] == 'D')
                        {
                            if(pboard->pplayers->player[i].in_bushes) pboard->pplayers->player[i].in_bushes = 0; //If player is in bush wait...
                            else
                            {
                                pboard->pplayers->player[i].y++;moved=1;
                                if(pboard->map[p_y+1][p_x] == 'c') {pboard->pplayers->player[i].carry_coins++; pboard->map[p_y+1][p_x] = ' ';}
                                if(pboard->map[p_y+1][p_x] == 't') {pboard->pplayers->player[i].carry_coins+=10; pboard->map[p_y+1][p_x] = ' ';}
                                if(pboard->map[p_y+1][p_x] == 'T') {pboard->pplayers->player[i].carry_coins+=50; pboard->map[p_y+1][p_x] = ' ';}
                                if(pboard->map[p_y+1][p_x] == 'A') {pboard->pplayers->player[i].safe_coins += pboard->pplayers->player[i].carry_coins; pboard->pplayers->player[i].carry_coins = 0;}
                                if(pboard->map[p_y+1][p_x] == '#')
                                        pboard->pplayers->player[i].in_bushes = 1;
                                for(int n = 0; n < BAGS; n++)
                                {
                                    if(p_x == pboard->bags[n].x && p_y+1 == pboard->bags[n].y)
                                    {
                                        pboard->pplayers->player[i].carry_coins += pboard->bags[n].amount;
                                        pboard->bags[n].x = -1;
                                        pboard->bags[n].y = -1;
                                        break;
                                    }
                                }
                            }
                        }
                if(pboard->pplayers->player[i].move == 'D' || pboard->pplayers->player[i].move == 'a') //Move left
                    if(pboard->map[p_y][p_x-1] == ' ' || pboard->map[p_y][p_x-1] == 'c' || pboard->map[p_y][p_x-1] == 't' || pboard->map[p_y][p_x-1] == 'T' || pboard->map[p_y][p_x-1] == '#' || pboard->map[p_y][p_x-1] == 'A' || pboard->map[p_y][p_x-1] == 'D')
                        {
                            if(pboard->pplayers->player[i].in_bushes) pboard->pplayers->player[i].in_bushes = 0; //If player is in bush wait...
                            else
                            {
                                pboard->pplayers->player[i].x--;moved=1;
                                if(pboard->map[p_y][p_x-1] == 'c') {pboard->pplayers->player[i].carry_coins++; pboard->map[p_y][p_x-1] = ' ';}
                                if(pboard->map[p_y][p_x-1] == 't') {pboard->pplayers->player[i].carry_coins+=10; pboard->map[p_y][p_x-1] = ' ';}
                                if(pboard->map[p_y][p_x-1] == 'T') {pboard->pplayers->player[i].carry_coins+=50; pboard->map[p_y][p_x-1] = ' ';}
                                if(pboard->map[p_y][p_x-1] == 'A') {pboard->pplayers->player[i].safe_coins += pboard->pplayers->player[i].carry_coins; pboard->pplayers->player[i].carry_coins = 0;}
                                if(pboard->map[p_y][p_x-1] == '#')
                                    pboard->pplayers->player[i].in_bushes = 1;
                                for(int n = 0; n < BAGS; n++)
                                {
                                    if(p_x-1 == pboard->bags[n].x && p_y == pboard->bags[n].y)
                                    {
                                        pboard->pplayers->player[i].carry_coins += pboard->bags[n].amount;
                                        pboard->bags[n].x = -1;
                                        pboard->bags[n].y = -1;
                                        break;
                                    }
                                }
                            }
                        }
                if(pboard->pplayers->player[i].move == 'C' || pboard->pplayers->player[i].move == 'd') // Move right
                    if(pboard->map[p_y][p_x+1] == ' ' || pboard->map[p_y][p_x+1] == 'c' || pboard->map[p_y][p_x+1] == 't' || pboard->map[p_y][p_x+1] == 'T' || pboard->map[p_y][p_x+1] == '#' || pboard->map[p_y][p_x+1] == 'A' || pboard->map[p_y][p_x-1] == 'D')
                    {
                        if(pboard->pplayers->player[i].in_bushes) pboard->pplayers->player[i].in_bushes = 0; //If player is in bush wait...
                        else
                        {
                            pboard->pplayers->player[i].x++;moved=1;
                            if(pboard->map[p_y][p_x+1] == 'c') {pboard->pplayers->player[i].carry_coins++; pboard->map[p_y][p_x+1] = ' ';}
                            if(pboard->map[p_y][p_x+1] == 't') {pboard->pplayers->player[i].carry_coins+=10; pboard->map[p_y][p_x+1] = ' ';}
                            if(pboard->map[p_y][p_x+1] == 'T') {pboard->pplayers->player[i].carry_coins+=50; pboard->map[p_y][p_x+1] = ' ';}
                            if(pboard->map[p_y][p_x+1] == 'A') {pboard->pplayers->player[i].safe_coins += pboard->pplayers->player[i].carry_coins; pboard->pplayers->player[i].carry_coins = 0;}
                            if(pboard->map[p_y][p_x+1] == '#')
                                        pboard->pplayers->player[i].in_bushes = 1;
                            for(int n = 0; n < BAGS; n++)
                            {
                                if(p_x+1 == pboard->bags[n].x && p_y == pboard->bags[n].y)
                                {
                                    pboard->pplayers->player[i].carry_coins += pboard->bags[n].amount;
                                    pboard->bags[n].x = -1;
                                    pboard->bags[n].y = -1;
                                    break;
                                }
                            }
                        }
                    }

                int collision = 0;
                for(int i = 0; i < 4; i++) //Check if there is any collision
                {
                    for(int j = 0; j < MAX_BEASTS; j++)
                    {
                        if(!pboard->pplayers->beast[j].in_game) break;
                        if(pboard->pplayers->player[i].x == pboard->pplayers->beast[j].x && pboard->pplayers->player[i].y == pboard->pplayers->beast[j].y)
                        {
                            if(pboard->pplayers->player[i].carry_coins != 0)
                            {
                                for(int n = 0; n < BAGS; n++) //Make bag with coins
                                {
                                    if(pboard->bags[n].x == -1)
                                    {
                                        pboard->bags[n].x = pboard->pplayers->player[i].x;
                                        pboard->bags[n].y = pboard->pplayers->player[i].y;
                                        pboard->bags[n].amount = pboard->pplayers->player[i].carry_coins;
                                        break;
                                    }
                                }
                            }
                            pboard->pplayers->player[i].deaths++;
                            pboard->pplayers->player[i].carry_coins = 0;
                            new_pos(pboard, i);
                        }
                    }
                    int j = 1;
                    for(; j < 3; j++)
                    {
                        if(j > i && pboard->pplayers->player[i].in_game && pboard->pplayers->player[j].in_game)
                        {
                            if(pboard->pplayers->player[i].x == pboard->pplayers->player[j].x && pboard->pplayers->player[i].y == pboard->pplayers->player[j].y)
                            {
                                collision = 1;
                                pboard->pplayers->player[i].deaths++;
                                pboard->pplayers->player[j].deaths++;
                                if(pboard->pplayers->player[i].carry_coins == 0 && pboard->pplayers->player[j].carry_coins == 0)
                                {
                                    for(int n = 0; n < BAGS; n++) //Make bag with coins
                                    {
                                        if(pboard->bags[n].x == -1)
                                        {
                                            pboard->bags[n].x = pboard->pplayers->player[i].x;
                                            pboard->bags[n].y = pboard->pplayers->player[i].y;
                                            pboard->bags[n].amount = pboard->pplayers->player[i].carry_coins + pboard->pplayers->player[j].carry_coins;
                                            break;
                                        }
                                    }
                                }
                                pboard->pplayers->player[i].carry_coins = 0;
                                pboard->pplayers->player[j].carry_coins = 0;
                                break;
                            }
                        }
                    }
                    if(collision) //Give new position to dead players
                    {
                        new_pos(pboard, i);
                        new_pos(pboard, j);
                        break;
                    }
                }

                if(pboard->pplayers->player[i].move == 'q') //Player exit game
                {
                    pboard->pplayers->player[i].in_game = 0;
                    pboard->pplayers->player[i].x = -1;
                    pboard->pplayers->player[i].move = 'x';
                    pboard->pplayers->player[i].carry_coins = 0;
                    pboard->pplayers->player[i].safe_coins = 0;
                    pboard->pplayers->player[i].deaths = 0;
                }
                else
                {
                    int x = pboard->pplayers->player[i].x - 2;
                    int y = pboard->pplayers->player[i].y - 2;

                    if(moved)
                        pboard->pplayers->player[i].move = 'x';

                    int curr_x = 0, curr_y = 0;
                    for(int j = 0; j < 25; j++)
                    {
                        if(x >= 0 && y >= 0)
                        {
                            if(x == pboard->camp_x && y == pboard->camp_y)
                                pboard->pplayers->player[i].map[curr_y][curr_x] = 'A';
                            else
                                pboard->pplayers->player[i].map[curr_y][curr_x] = pboard->map[y][x];
                            for(int n = 0; n < BAGS; n++) //Check if there are any bags nearby
                            {
                                if(x == pboard->bags[n].x && y == pboard->bags[n].y)
                                    pboard->pplayers->player[i].map[curr_y][curr_x] = 'D';
                            }
                        }
                        else
                        {
                            pboard->pplayers->player[i].map[curr_y][curr_x] = 'X';
                        }
                        for(int n = 0; n < 4; n++) //Check if there are other players nearby
                        {
                            if(pboard->pplayers->player[n].in_game)
                            {
                                if(n != i && x == pboard->pplayers->player[n].x && y == pboard->pplayers->player[n].y)
                                    pboard->pplayers->player[i].map[curr_y][curr_x] = '0' + n;
                            }
                        }
                        curr_x++, x++;
                        if(curr_x > 4)
                        {
                            curr_x = 0, x -= 5;
                            curr_y++, y++;
                        }
                    }
                }
            }
        }
        for(int i = 0; i < 4; i++)
            if(pboard->pplayers->player[i].in_game)
                sem_post(&pboard->pplayers->player[i].request_move);
        sem_post(&pboard->pplayers->sync_map);
        usleep(1000000);
    }

    return NULL;
}

void new_pos(struct board_t *board, int id)
{
    int x = 0, y = 0;
    while(board->map[y][x] != ' ')
    {
        x = rand() % MAP_SIZE_X;
        y = rand() % MAP_SIZE_Y;
    }
    board->pplayers->player[id].x = x;
    board->pplayers->player[id].y = y;
    return;
}

void *resp_player(void *players)
{
    struct board_t *pboard = (struct board_t *)players;

    while(1)
    {
        sem_wait(&pboard->pplayers->player_response);
        for(int i = 0; i < 4; i++)
        {
            if(pboard->pplayers->player[i].x == 0)
            {
                pboard->pplayers->player[i].safe_coins = 0;
                pboard->pplayers->player[i].carry_coins = 0;
                pboard->pplayers->player[i].deaths = 0;
                pboard->pplayers->player[i].in_bushes = 0;
                int x = 0, y = 0;
                while(pboard->map[y][x] != ' ')
                {
                    x = rand() % MAP_SIZE_X;
                    y = rand() % MAP_SIZE_Y;
                }
                pboard->pplayers->player[i].x = x;
                pboard->pplayers->player[i].y = y;

                x = pboard->pplayers->player[i].x - 2;
                y = pboard->pplayers->player[i].y - 2;

                int curr_x = 0, curr_y = 0;
                for(int j = 0; j < 25; j++)
                {
                    if(x >= 0 && y >= 0)
                    {
                        pboard->pplayers->player[i].map[curr_y][curr_x] = pboard->map[y][x];
                    }
                    else
                    {
                        pboard->pplayers->player[i].map[curr_y][curr_x] = 'X';
                    }
                    curr_x++, x++;
                    if(curr_x > 4)
                    {
                        curr_x = 0, x -= 5;
                        curr_y++, y++;
                    }
                }
                pboard->pplayers->player[i].in_game = 1;
                sem_post(&pboard->pplayers->request_position);
            }
        }
    }
    return NULL;
}

void add_coin(struct board_t *board, int count, char *type)
{
    for(int i = 0; i < count; i++)
    {
        int x = 0, y = 0;
        while(board->map[y][x] != ' ')
        {
            x = rand() % MAP_SIZE_X;
            y = rand() % MAP_SIZE_Y;
        }
        if(type != NULL) board->map[y][x] = *type;
        else
        {
            int random = rand() % 3;
            if(random == 0) board->map[y][x] = 'c';
            if(random == 1) board->map[y][x] = 't';
            if(random == 2) board->map[y][x] = 'T';
        }
    }
}

void *take_input(void *something)
{
    struct board_t *ptr = (struct board_t *)something;
    char sign;
    while(1)
    {
        sign = getchar();
        if(sign == 'q') break;
        if(sign == 'c' || sign == 't' || sign == 'T') //Add a new coin to map
        {
            pthread_mutex_lock(&ptr->add_coin);
            add_coin(ptr, 1, &sign);
            pthread_mutex_unlock(&ptr->add_coin);
        }
        if(sign == 'b' || sign  == 'B')
        {
            for(int i = 0; i < MAX_BEASTS; i++)
            {
                if(!ptr->pplayers->beast[i].in_game)
                {
                    ptr->pplayers->beast[i].in_game = 1;
                    break;
                }
            }
        }
    }
    endwin();
    sem_close(&ptr->pplayers->player_response);
    sem_close(&ptr->pplayers->request_position);
    sem_close(&ptr->pplayers->sync_map);
    for(int i = 0; i < 4; i++)
    {
        sem_close(&ptr->pplayers->player[i].request_move);
    }
    pthread_mutex_destroy(&ptr->add_coin);
    munmap(ptr->pplayers, sizeof(struct players_t));
    shm_unlink("/players");
    free(ptr);
    exit(0);
    return NULL;
}

void new_map(char *filename, char tab[MAP_SIZE_Y][MAP_SIZE_X])
{
    FILE *plik = fopen(filename, "r");

    int x = 0;
    int y = 0;
    while(1)
    {
        char sign;
        fscanf(plik, "%c", &sign);
        if(feof(plik))  break;
        if(sign == '\n')
        {
            x = 0;
            y++;
        }
        tab[y][x] = sign;
        x++;
    }
}
