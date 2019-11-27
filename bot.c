#include <stdio.h>
#include <ncurses.h>
#include "info.h"

void *take_input(void *player);
void *move_bot(void *player);

int main()
{
    srand(time(NULL));
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


    int players_data;
    while(1)   //Try to open data
    {
        players_data = shm_open("/players", O_RDWR, 0600);
        if(players_data != -1)  break;
        mvprintw(0, 0, "Waiting for server ...");
        refresh();
        usleep(1000000);
    }

    struct players_t *players = (struct players_t *)mmap(NULL, sizeof(struct players_t), PROT_READ | PROT_WRITE, MAP_SHARED, players_data, 0);
    int my_id = 0;
    for(int i = 0; i < 4; i++) //Find id
    {
        if(players->player[i].in_game == 0)
        {
            players->player[i].type = BOT;
            players->player[i].x = 0;
            players->player[i].in_game = 0;
            my_id = i;
            sem_post(&players->player_response);
            sem_wait(&players->request_position);
            break;
        }
    }


    int start = 1;

    pthread_t input_thread, bot_thread;
    pthread_create(&input_thread, NULL, take_input, &players->player[my_id]); //Thread for taking input
    pthread_create(&bot_thread, NULL, move_bot, &players->player[my_id]); //Thread for moving bot

    while(1)
    {
        if(!start)
        {
            sem_wait(&players->player[my_id].request_move);
        }
        clear();
        int x = players->player[my_id].x - 2;
        int y = players->player[my_id].y - 2;
        int check_x = x + 5, curr_x = 0, curr_y = 0;
        for(int i = 0; i < 25; i++)
        {
            if(x >= 0 && y >= 0)
            {
                if(players->player[my_id].map[curr_y][curr_x] == '|') //Print walls
                {
                    attron(COLOR_PAIR(2));
                    mvprintw(y, x, ".");
                }
                if(players->player[my_id].map[curr_y][curr_x] == ' ') //Print empty spaces
                {
                    attron(COLOR_PAIR(1));
                    mvprintw(y, x, "0");
                }
                if(players->player[my_id].map[curr_y][curr_x] == '#') //Print bushes
                {
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "#");
                }
                if(players->player[my_id].map[curr_y][curr_x] == 'A') //Print campsite
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(y, x, "A");
                }
                if(players->player[my_id].map[curr_y][curr_x] == 'c' || players->player[my_id].map[curr_y][curr_x] == 't' || players->player[my_id].map[curr_y][curr_x] == 'T' || players->player[my_id].map[curr_y][curr_x] == 'D')
                {                                                    //Print treasure
                    attron(COLOR_PAIR(5));
                    mvprintw(y, x, "%c", players->player[my_id].map[curr_y][curr_x]);
                }
                if(x == players->player[my_id].x && y == players->player[my_id].y)
                {
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "%c", '1' + my_id);
                }
                if(players->player[my_id].map[curr_y][curr_x] == '0' || players->player[my_id].map[curr_y][curr_x] == '1' || players->player[my_id].map[curr_y][curr_x] == '2' || players->player[my_id].map[curr_y][curr_x] == '3')
                {                                                    //Print players
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "%c", players->player[my_id].map[curr_y][curr_x]);
                }
                for(int j = 0; j < MAX_BEASTS; j++)                 //Print beasts
                {
                    if(!players->beast[j].in_game) break;
                    if(x == players->beast[j].x && y == players->beast[j].y)
                    {
                        attron(COLOR_PAIR(8));
                        mvprintw(y, x, "*");
                    }
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
        attron(COLOR_PAIR(6));                      //Print Legend
        mvprintw(0, 54, "Player:"); mvprintw(0, 68, "%d", my_id + 1);
        mvprintw(1, 54, "Type:"); mvprintw(1, 68, "HUMAN");
        mvprintw(3, 54, "Safe coins:"); mvprintw(3, 68, "%d", players->player[my_id].safe_coins);
        mvprintw(4, 54, "Coins with:"); mvprintw(4, 68, "%d", players->player[my_id].carry_coins);
        mvprintw(5, 54, "Deaths:"); mvprintw(5, 68, "%d", players->player[my_id].deaths);
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

void *move_bot(void *player)
{
    struct player_t *p = (struct player_t *)player;

    while(1)
    {   //Randomize bot movement
        int random = rand() % 4;
        if(random == 0) p->move = 'a';
        else if(random == 1) p->move = 's';
        else if(random == 2) p->move = 'd';
        else if(random == 3) p->move = 'w';
        usleep(500000);
    }
    return NULL;
}

void *take_input(void *player) //Function to disconnect bot when 'q' was pressed
{
    struct player_t *p = (struct player_t *)player;

    char move = 'x';

    while(move != 'q')
    {
        move = getchar();
        if(move == 'q') 
        {
            p->move = move;
            break;
        }
    }
    endwin();
    exit(0);
    return NULL;
}