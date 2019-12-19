#include <stdio.h>
#include <ncurses.h>
#include "info.h"

void *take_input(void *player);

int main()
{
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
    init_pair(6, COLOR_WHITE, -1);
    init_pair(7, COLOR_YELLOW, COLOR_GREEN);
    init_pair(8, COLOR_RED, COLOR_WHITE);

    int players_id;
    while(1)
    {
        players_id = shm_open("/id", O_RDWR, 0600);
        if(players_id != -1) break;
        clear();
        mvprintw(0, 0, "Waiting for server ...");
        refresh();
        usleep(1000000);
    }

    struct req_id *p_id = (struct req_id *)mmap(NULL, sizeof(struct req_id)*4, PROT_READ | PROT_WRITE, MAP_SHARED, players_id, 0);

    int my_id = 0;
    while(1)
    {
        sem_post(&p_id->player_response);
        sem_wait(&p_id->request_position);
        int found = 0;
        for(int i = 0; i < 4; i++) //Find id
        {
            if(p_id->id[i] == 1)
            {
                p_id->id[i] = 2;
                my_id = i;
                found = 1;
                break;
            }
        }
        if(found) break;
        else
        {
            clear();
            mvprintw(0, 0, "Server is full, please wait...");
            refresh();
            usleep(1000000);
        }
    }

    int players_data;
    while(1) //Open data
    {
        if(my_id == 0)
            players_data = shm_open("/player1", O_RDWR, 0600);
        if(my_id == 1)
            players_data = shm_open("/player2", O_RDWR, 0600);
        if(my_id == 2)
            players_data = shm_open("/player3", O_RDWR, 0600);
        if(my_id == 3)
            players_data = shm_open("/player4", O_RDWR, 0600);
        if(players_data != -1)  break;
        mvprintw(0, 0, "Waiting for server ...");
        refresh();
        usleep(1000000);
    }
    struct player_t *player = (struct player_t *)mmap(NULL, sizeof(struct player_t), PROT_READ | PROT_WRITE, MAP_SHARED, players_data, 0);
    player->type = HUMAN;

    int start = 1;
    pthread_t input_thread;
    pthread_create(&input_thread, NULL, take_input, player); //Thread for taking input and moving

    while(1)
    {
        if(!start) //Request move and wait for server
        {
            sem_wait(&player->request_move);
        }
        clear();
        int x = player->x - 2;
        int y = player->y - 2;
        int check_x = x + 5, curr_x = 0, curr_y = 0;
        for(int i = 0; i < 25; i++)
        {
            if(x >= 0 && y >= 0)
            {
                if(player->map[curr_y][curr_x] == '|') //Print walls
                {
                    attron(COLOR_PAIR(2));
                    mvprintw(y, x, ".");
                }
                if(player->map[curr_y][curr_x] == ' ') //Print empty spaces
                {
                    attron(COLOR_PAIR(1));
                    mvprintw(y, x, "0");
                }
                if(player->map[curr_y][curr_x] == '#') //Print bushes
                {
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "#");
                }
                if(player->map[curr_y][curr_x] == 'A') //Print campsite
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(y, x, "A");
                }
                if(player->map[curr_y][curr_x] == 'c' || player->map[curr_y][curr_x] == 't' || player->map[curr_y][curr_x] == 'T' || player->map[curr_y][curr_x] == 'D')
                {                                                     //Print treasure
                    attron(COLOR_PAIR(5));
                    mvprintw(y, x, "%c", player->map[curr_y][curr_x]);
                }
                if(x == player->x && y == player->y) //Print player position
                {           
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "%c", '1' + my_id);
                }
                if(player->map[curr_y][curr_x] == '1' || player->map[curr_y][curr_x] == '2' || player->map[curr_y][curr_x] == '3' || player->map[curr_y][curr_x] == '4')
                {                                                       //Print other players
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "%c", player->map[curr_y][curr_x]);
                }
                if(player->map[curr_y][curr_x] == '*')
                {
                    attron(COLOR_PAIR(8));
                    mvprintw(y, x, "*");
                }  
                refresh();
            }
            x++, curr_x++;
            if(x == check_x)
            {
                x -= 5, curr_x = 0;
                y++, curr_y++;;
            }
        }
        attron(COLOR_PAIR(6));              //Print Legend
        mvprintw(0, 54, "Player:"); mvprintw(0, 68, "%d", my_id + 1);
        mvprintw(1, 54, "Type:"); mvprintw(1, 68, "HUMAN");
        mvprintw(3, 54, "Safe coins:"); mvprintw(3, 68, "%d", player->safe_coins);
        mvprintw(4, 54, "Coins with:"); mvprintw(4, 68, "%d", player->carry_coins);
        mvprintw(5, 54, "Deaths:"); mvprintw(5, 68, "%d", player->deaths);
        mvprintw(10, 54, "Legend:");
        attron(COLOR_PAIR(4)); mvprintw(11, 54, "1234"); attron(COLOR_PAIR(6)); mvprintw(11, 58, " - Players");
        attron(COLOR_PAIR(2)); mvprintw(12, 54, "|"); attron(COLOR_PAIR(6)); mvprintw(12, 55, "     - Wall");
        mvprintw(13, 54, "#     - Bushes");
        mvprintw(14, 54, "*     - Enemy");
        attron(COLOR_PAIR(5)); mvprintw(15, 54, "c"); attron(COLOR_PAIR(6)); mvprintw(15, 55, "     - One coin");
        attron(COLOR_PAIR(5)); mvprintw(16, 54, "t"); attron(COLOR_PAIR(6)); mvprintw(16, 55, "     - Treasure (10 coins)");
        attron(COLOR_PAIR(5)); mvprintw(17, 54, "T"); attron(COLOR_PAIR(6)); mvprintw(17, 55, "     - Large treasure (50 coins)");
        attron(COLOR_PAIR(7)); mvprintw(18, 54, "A"); attron(COLOR_PAIR(6)); mvprintw(18, 55, "     - Campsite");
        refresh();
        start = 0;
    }
    getchar();
    endwin();
    close(players_data);
    return 0;
}

void *take_input(void *player)
{
    struct player_t *p = (struct player_t *)player;

    char move = 'z';

    while(move != 'q')
    {
        move = getchar();
        p->move = move;
        if(move == 'q') 
            break;
    }
    endwin();
    exit(0);
    return NULL;
}
