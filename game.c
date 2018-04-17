#include <unistd.h>
#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

enum { scr_width = 60 };
enum { scr_height = 24 };
enum { key_escape = 27 };
enum { 
	b_empty, b_armor, b_wall, b_player, 
	b_bomb, b_door_closed, b_door_opened,
	b_enemy
};
enum { one_tick = 30 };
enum { bomb_delay = 7 };
enum { bomb_radius = 2 };
enum { num_of_doors = 3 };
enum { max_path_len = 100 };
const double delay = 0.5;

struct moving_obj {
	int x;
	int y;
	int health;
	/*used only by player*/
	int dx;
	int dy;
	int is_moving;
	/*used only by enemies*/
	int step;
	char *path;
};

/*list of enemies*/
struct e_list {
	struct moving_obj *enemy;
	struct e_list *next;
};

struct p_bomb {
	int x;
	int y;
	int timer;
	int radius;
};

/*game status*/
struct status {
	struct p_bomb *bomb;
	struct e_list *enemies;
	struct moving_obj *player;

};

void draw_char(int x, int y, char c)
{
	move(y, x);
	addch(c);
	curs_set(0);
	refresh();
}

/*reads map from txt file*/
char *map_init(char *map, const char *name)
{
	int i, j;
	FILE *level;
	map = malloc(scr_height * scr_width);
	level = fopen(name, "r");
	if(level == NULL) {
		perror(name);
	}
	/*number of strings with no-map info*/
	i = fgetc(level)-'0';
	/*reads '\n'*/
	fgetc(level);
	/*ignores no-map strings*/
	for( ; i > 0; i--) {
		char c;
		while((c = fgetc(level)) != '\n')
			{}
	}
	/*reads map*/
	for(i = 0; i < scr_height; i++) {
		for(j = 0; j < scr_width; j++) {
			map[(i-1)*scr_width + j] = fgetc(level) - '0';
		}
		fgetc(level);
	}
	fclose(level);
	return map;
}

/*randomly places doors on map*/
void place_doors(char *map)
{
	int i;
	srand(time(NULL));
	for(i = 0; i < num_of_doors; i++)
	{
		int q = 0;
		while(map[q] != b_wall) {
			double size = scr_height*scr_width;
			/*gets random number*/
			q = 1+(int)(size*rand()/(RAND_MAX+1.0));
		}
		map[q] = b_door_closed;
	}
}

void map_draw(const char *map)
{
	int i, j;
	for(i = 0; i < scr_height; i++) {
		for(j = 0; j < scr_width; j++) {
			switch(map[(i-1)*scr_width + j]) {
			case b_empty:
				draw_char(j, i, ' ');
				break;
			case b_armor:
				draw_char(j, i, '@');
				break;
			case b_wall:
			case b_door_closed:
				draw_char(j, i, ':');
				break;
			case b_player:
				draw_char(j, i, 'X');
				break;
			case b_bomb:
				draw_char(j, i, '*');
				break;
			case b_door_opened:
				draw_char(j, i, 'O');
				break;
			case b_enemy:
				draw_char(j, i, '&');
				break;
			}
		}
	}
	refresh();
}

/*creates new moving object*/
struct moving_obj *player_init(struct moving_obj *player,
				int x, int y, char *map)
{
	player = malloc(sizeof(*player));
	player->x = x;
	player->y = y;
	player->dx = 0;
	player->dy = -1;
	player->health = 1;
	player->is_moving = 0;
	map[(y-1)*scr_width + x] = b_player;
	return player;
}

/*takes info about player from level txt file*/
struct moving_obj *player_from_file(struct moving_obj *player,
					const char *name, char *map)
{
	FILE *level;
	level = fopen(name, "r");
	if(level == NULL) {
		perror(name);
	}
	fgetc(level);
	fgetc(level);
	int x, y;
	x = (fgetc(level)-'0')*10 + fgetc(level)-'0';
	y = (fgetc(level)-'0')*10 + fgetc(level)-'0';
	player = player_init(player, x, y, map);
	return player;
}

/*moves char to new position in map*/
void move_char(int *x, int *y, int dx, int dy, char *map)
{
	int b;
	b = map[(*y-1)*scr_width + *x];
	map[(*y-1)*scr_width + *x] = b_empty;
	*x += dx;
	*y += dy;
	map[(*y-1)*scr_width + *x] = b;
}

/*checks the posibility to go forward*/
int check_move(const struct moving_obj *obj, const char *map)
{
	int flag = 1;
	int x, y, dx, dy;
	x = obj->x;
	y = obj->y;
	dx = obj->dx;
	dy = obj->dy;
	x += dx;
	y += dy;
	int q = (y-1)*scr_width + x;
	/*if there is not space on map*/
	if(map[q] != b_empty) {
		if(map[q] == b_door_opened) {
			endwin();
			printf("YOU WIN!!!\n");
			exit(0);
		}
		flag = 0;
	}
	return flag;
}

void param_init()
{
	initscr();
	cbreak();
	keypad(stdscr, 1);
	noecho();
	timeout(0);
}

/*creates bomb*/
struct p_bomb *make_bomb(const struct moving_obj *p,
				struct p_bomb *bomb, char *map)
{
	int x, y;
	x = p->x + p->dx;
	y = p->y + p->dy;
	/*checks is it possibly to put bomb*/
	if(map[(y-1)*scr_width + x] == 0) {
		/*if there are no player bomb*/
		if(bomb == NULL) {
			bomb = malloc(sizeof(*bomb));
			bomb->x = x;
			bomb->y = y;
			bomb->timer = bomb_delay;
			bomb->radius = bomb_radius;
			map[(bomb->y - 1)*scr_width + bomb->x] = b_bomb;
		}
	}
	return bomb;
}

/*makes action in case of key*/
struct p_bomb *key_action(struct status game, int key, char *map)
{
	struct moving_obj *player = game.player;
	player->is_moving = 0;
	switch(key) {
	case KEY_UP:
		player->is_moving = 1;
		player->dy = -1;
		player->dx = 0;
		break;
	case KEY_RIGHT:
		player->is_moving = 1;
		player->dx = +1;
		player->dy = 0;
		break;
	case KEY_DOWN:
		player->is_moving = 1;
		player->dy = +1;
		player->dx = 0;
		break;
	case KEY_LEFT:	
		player->is_moving = 1;
		player->dx = -1;
		player->dy = 0;
		break;
	case ' ':
		game.bomb = make_bomb(game.player, game.bomb, map);
		break;
	}
	return game.bomb;
}

/*deletes enemy from list*/
struct status kill_enemy(struct status game, int x, int y)
{
	struct e_list *current = game.enemies;
	struct e_list *previous = NULL;
	/*finds which enemy was killed*/
	while(current) {
		if(current->enemy->x == x && current->enemy->y == y) {
			if(previous) {
				previous->next = current->next;
			} else {
				game.enemies = current->next;
			}
			free(current->enemy);
			free(current);
			break;
		}
		previous = current;
		current = current->next;
	}
	return game;
}

/*makes bomb bang in the given direction*/
struct status bomb_bang(struct status game, int dx, int dy, char *map)
{
	int x, y;
	x = game.bomb->x;
	y = game.bomb->y;
	int i;
	for(i = 0; i < game.bomb->radius; i++) {
		y += dy;
		x += dx;
		int q = (y-1)*scr_width + x;
		/*is it possibly to destroy*/
		if(map[q] != b_armor) {
			if(map[q] == b_door_closed) {
				/*opens the door if it is founded*/
				map[q] = b_door_opened;
			} else if(map[q] == b_player) {
				/*destroys the player*/
				game.player->health = 0;
			} else if(map[q] == b_enemy) {
				game = kill_enemy(game, x, y);
				map[q] = b_empty;
			} else {
				map[q] = b_empty;
			}
		} else {
			/*stop destroing if it is armor*/
			break;
		}
	}
	return game;
}

/*bomb actions*/
struct status bomb_tick(struct status game, char *map)
{
	struct p_bomb *bomb = game.bomb;
	bomb->timer--;
	if(bomb->timer <= 0) {
		map[(bomb->y - 1)*scr_width + bomb->x] = 0;
		/*these are to clear lines in four direction*/
		game = bomb_bang(game, +1, +0, map);
		game = bomb_bang(game, -1, +0, map);
		game = bomb_bang(game, +0, +1, map);
		game = bomb_bang(game, +0, -1, map);
		/*these loops are to clear all square*/
		int i;
		for (i = 0; i < bomb->radius; i++) {
			bomb->x++;
			game = bomb_bang(game, +0, +1, map);
			game = bomb_bang(game, +0, -1, map);
		}
		bomb->x = bomb->x - bomb->radius;
		for (i = 0; i < bomb->radius; i++) {
			bomb->x--;
			game = bomb_bang(game, +0, +1, map);
			game = bomb_bang(game, +0, -1, map);
		}
		free(bomb);
		bomb = NULL;
	}
	/*because pointer can be changed*/
	game.bomb = bomb;
	return game;
}

struct status game_init(struct moving_obj *player,
			struct e_list *enemies, struct p_bomb *bomb)
{
	struct status game;
	game.player = player;
	game.enemies = enemies;
	game.bomb = bomb;
	return game;
}

/*adds new enemy to list*/
struct e_list *add_enemy(struct e_list *enemies,
				char *path, int x, int y, int is_moving)
{
	struct moving_obj *enemy;
	enemy = malloc(sizeof(*enemy));
	enemy->health = 1;
	enemy->x = x;
	enemy->y = y;
	enemy->dx = 0;
	enemy->dy = 0;
	enemy->is_moving = is_moving;
	enemy->step = 0;
	enemy->path = path;
	struct e_list *new;
	new = malloc(sizeof(new));
	new->enemy = enemy;
	new->next = enemies;
	enemies = new;
	return enemies;
}

/*reads enemy configurations from level txt file*/
struct e_list *enemies_from_file(struct e_list *enemies, const char *name)
{
	FILE *level;
	level = fopen(name, "r");
	if(level == NULL) {
		perror(name);
	}
	/*reads number of enemies*/
	/*-1 cause of player info*/
	int i = fgetc(level) - '0' - 1;
	/*reads \n*/
	fgetc(level);
	int c;
	/*ignores player info*/
	while((c = fgetc(level)) != '\n')
		{}
	for( ; i > 0; i--) {
		int x, y, is_moving;
		char *path, *cur;
		path = malloc(max_path_len);
		x = (fgetc(level)-'0')*10 + fgetc(level)-'0';
		y = (fgetc(level)-'0')*10 + fgetc(level)-'0';
		is_moving = fgetc(level)-'0';
		int c;
		cur = path;
		while((c = fgetc(level)) != '\n') {
			*cur = c-'0';
			cur++;
		}
		/*end of string*/
		*cur = 0;
		enemies = add_enemy(enemies, path, x, y, is_moving);
	}
	fclose(level);
	return enemies;
}

/*adds enemies to map*/
void enemy_to_map(struct e_list *enemies, char *map)
{
	struct e_list *current = enemies;
	while(current) {
		int x, y;
		x = current->enemy->x;
		y = current->enemy->y;
		map[(y-1)*scr_width + x] = b_enemy;
		current = current->next;
	}
}

/*checks is player on near position*/
void player_near(struct moving_obj *enemy, int dx, int dy, char *map)
{
	int x = enemy->x;
	int y = enemy->y;
	if(map[(y+dy-1)*scr_width+x+dx] == b_player) {
		enemy->dx = dx;
		enemy->dy = dy;
	}
}

/*analyzes next path symbol*/
void next_step(int *dx, int *dy, char *path, int step)
{
	switch(path[step]) {
	case 1:
		*dx = +0;
		*dy = -1;
		break;
	case 2:
		*dx = +1;
		*dy = +0;
		break;
	case 3:
		*dx = +0;
		*dy = +1;
		break;
	case 4:
		*dx = -1;
		*dy = +0;
		break;
	}
}

/*enemy actions*/
struct status enemies_tick(struct status game, char *map)
{
	struct e_list *current = game.enemies;
	while(current) {
		struct moving_obj *enemy = current->enemy;
		if(enemy->path[enemy->step] == 0) {
			enemy->step = 0;
		}
		next_step(&enemy->dx, &enemy->dy, enemy->path, enemy->step);
		player_near(enemy, +0, -1, map);
		player_near(enemy, +1, +0, map);
		player_near(enemy, +0, +1, map);
		player_near(enemy, -1, +0, map);
		/*q = next enemy position*/
		int q = (enemy->y+enemy->dy-1)*scr_width+enemy->x+enemy->dx;
		if(map[q] == b_player) {
			game.player->health = 0;
		}
		if(map[q] != b_bomb) {
			move_char(&enemy->x, &enemy->y,
					enemy->dx, enemy->dy, map);
			enemy->step++;
		}
		current = current->next;
	}
	return game;
}

/*moves player if possibly*/
void move_player(struct moving_obj *player, char *map)
{
	int dx = player->dx;
	int dy = player->dy;
	if(check_move(player, map) && player->is_moving) {
		move_char(&player->x, &player->y, dx, dy, map);
	}
}

/*checks is player dead*/
void is_dead(int health, char *map)
{
	if(health == 0) {
		map_draw(map);
		sleep(delay*5);
		endwin();
		printf("YOU LOSE!!!\n");
		exit(0);
	}
}

void show_intro(const char *name)
{
	FILE *intro;
	intro = fopen(name, "r");
	int c;
	while((c = fgetc(intro)) != EOF) {
		addch(c);
	}
	refresh();
	curs_set(0);
	sleep(delay*5);
	move(2, 62);
	printw("%s"," This is a   ");
	move(3, 62);
	printw("%s","   Bomberman!    ");
	move(6, 62);
	printw("%s","                  ");
	move(7, 62);
	printw("%s","Rules:            ");
	move(8, 62);
	printw("%s","You can move 'X'  ");
	move(9, 62);
	printw("%s","by pressing keys. ");
	move(10, 62);
	printw("%s","Space to put bomb.");
	move(11, 62);
	printw("%s","'&' are enemies.  ");
	move(12, 62);
	printw("%s","You must find door");
	move(13, 62);
	printw("%s","'0' by destroing  ");
	move(14, 62);
	printw("%s","blocks.           ");
	move(20, 62);
	printw("%s","Game by:          ");
	move(21, 62);
	printw("%s","   Nikolay Titov ");

}

int main(int argc, char** argv)
{
	struct status game;
	struct p_bomb *bomb = NULL;
	char *map = NULL;
	int key;
	struct moving_obj *player = NULL;
	struct e_list *enemies = NULL;
	if(argv[1] == NULL) {
		printf("Please choose level\n");
		exit(1);
	}
	param_init();
	show_intro("intro.txt");
	/*creates map*/
	map = map_init(map, argv[1]);
	place_doors(map);
	/*creates player*/
	player = player_from_file(player, argv[1], map);
	enemies = enemies_from_file(enemies, argv[1]);
	enemy_to_map(enemies, map);
	game = game_init(player, enemies, bomb);
	map_draw(map);
	/*reading keys*/
	while(TRUE) {
		int timer;
		for(timer = 0; timer < one_tick; timer++) {
			key = getch();
			if(key == key_escape) {
				endwin();
				exit(0);
			}
			if(key != ERR) {
				game.bomb = key_action(game, key, map);
				/*move*/
				move_player(game.player, map);
			}
			map_draw(map);
			sleep(delay);
		}
		if(game.enemies != NULL) {
			game = enemies_tick(game, map);
		}
		if(game.bomb != NULL) {
			game = bomb_tick(game, map);
		}
		is_dead(game.player->health, map);
	}
	endwin();
	free(player);
	return 0;
}
