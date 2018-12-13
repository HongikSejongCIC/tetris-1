/*
 * Compilation: gcc -o tetris tetris.c
 */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // 불규칙 난수 생성을 위한 time 함수

#define ESC 27

#define DELAY 1
#define DELAY_FACTOR 0.8

#define RED 1
#define GREEN 2
#define YELLOW 3
#define BLUE 4
#define FUCHSIA 5
#define CYAN 6
#define WHITE 7

#define PLAYFIELD_W 10
#define PLAYFIELD_H 20
#define PLAYFIELD_X 30
#define PLAYFIELD_XX 120 // 2p 화면을 위한 x변위 추가.
#define PLAYFIELD_Y 1
#define BORDER_COLOR YELLOW

#define SCORE_X 1
#define SCORE_Y 2
#define SCORE_COLOR GREEN

#define HELP_X 58
#define HELP_XX 1 // 1p 조작법 위치
#define HELP_XXX 91 // 2p 조작법 위치
#define HELP_Y 1
#define HELP_COLOR CYAN

#define NEXT_X 14
#define NEXT_XX 104 // / 2p Next block 위치 지정을 위한 x변위 추가.
#define NEXT_Y 11

#define GAMEOVER_X 1
#define GAMEOVER_Y (PLAYFIELD_H + 3)

#define LEVEL_UP 20

#define FILLED_CELL "[]"
#define NEXT_EMPTY_CELL "  "
#define PLAYFIELD_EMPTY_CELL " ."

struct termios terminal_conf;
int use_color = 1;
long tetris_delay = DELAY * 1000000;
int attack1 = 0;
int attack2 = 0;

typedef struct {
    int origin_x;
    int origin_y;
    int x;
    int y;
    int color;
    int symmetry;
    int orientation;
    int *data;
    char empty_cell[3];
} tetris_piece_s;

void clear_screen() {
    printf("\033[2J");
}

void xyprint(int x, int y, char *s) {
    printf("\033[%d;%dH%s", y, x, s);
}

void show_cursor() {
    printf("\033[?25h");
}

void hide_cursor() {
    printf("\033[?25l");
}

void set_fg(int color) {
    if (use_color) {
        printf("\033[3%dm", color);
    }
}

void set_bg(int color) {
    if (use_color) {
        printf("\033[4%dm", color);
    }
}

void reset_colors() {
    printf("\033[0m");
}

void set_bold() {
    printf("\033[1m");
}

void cmd_quit() {
    int flags = fcntl(STDOUT_FILENO, F_GETFL);

    fcntl(STDOUT_FILENO, F_SETFL, flags & (~O_NONBLOCK));
    xyprint(GAMEOVER_X, GAMEOVER_Y, "Game over!");
    xyprint(GAMEOVER_X, GAMEOVER_Y + 1, "");
    show_cursor();
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal_conf);
    exit(0);
}

int *get_cells(tetris_piece_s piece, int *position) {
    static int cells[8] = {};
    int i = 0;
    int data = *(piece.data + piece.orientation);
    int x = piece.x;
    int y = piece.y;

    if (position) {
        x = *position;
        y = *(position + 1);
        data = *(piece.data + *(position + 2));
    }
    for (i = 0; i < 4; i++) {
        cells[2 * i] = x + ((data >> (4 * i)) & 3);
        cells[2 * i + 1] = y + ((data >> (4 * i + 2)) & 3);
    }
    return cells;
}

void draw_piece(tetris_piece_s piece, int visible) {
    int i = 0;
    int *cells = get_cells(piece, NULL);
    int x = 0;
    int y = 0;

    if (visible) {
        set_fg(piece.color);
        set_bg(piece.color);
    }
    for (i = 0; i < 4; i++) {
        x = cells[2 * i] * 2 + piece.origin_x;
        y = cells[2 * i + 1] + piece.origin_y;
        xyprint(x, y, visible ? FILLED_CELL : piece.empty_cell);
    }
    if (visible) {
        reset_colors();
    }
}

int position_ok(tetris_piece_s piece, int *playfield, int *position) {
    int i = 0;
    int x = 0;
    int y = 0;
    int *cells = get_cells(piece, position);

    for (i = 0; i < 4; i++) {
        x = *(cells + 2 * i);
        y = *(cells + 2 * i + 1);
        if (y < 0 || y >= PLAYFIELD_H || x < 0 || x >= PLAYFIELD_W || ((*(playfield + y) >> (x * 3)) & 7) != 0) {
            return 0;
        }
    }
    return 1;
}

int move(tetris_piece_s *piece, int *playfield, int dx, int dy, int dz) {
    int new_position[] = {piece->x + dx, piece->y + dy, (piece->orientation + dz) % piece->symmetry};

    if (position_ok(*piece, playfield, new_position)) {
        draw_piece(*piece, 0);
        piece->x = new_position[0];
        piece->y = new_position[1];
        piece->orientation = new_position[2];
        draw_piece(*piece, 1);
        return 1;
    }
    return (dy == 0);
}

void flatten_piece(tetris_piece_s *piece, int *playfield) {
    int i = 0;
    int x = 0;
    int y = 0;
    int *cells = get_cells(*piece, NULL);

    for (i = 0; i < 4; i++) {
        x = *(cells + 2 * i);
        y = *(cells + 2 * i + 1);
        *(playfield + y) |= (piece->color << (x * 3));
    }
}

void draw_playfield(int *playfield) {
    int x = 0;
    int y = 0;
    int color = 0;

    for (y = 0; y < PLAYFIELD_H; y++) {
        xyprint(PLAYFIELD_X, PLAYFIELD_Y + y, "");
        for (x = 0; x < PLAYFIELD_W; x++) {
            color = (*(playfield + y) >> (x * 3)) & 7;
            if (color) {
                set_bg(color);
                set_fg(color);
                printf(FILLED_CELL);
                reset_colors();
            } else {
                printf(PLAYFIELD_EMPTY_CELL);
            }
        }
    }
}

void draw_playfield1(int *playfield) { // playfield를 로드하여 2p 화면 구성 
	int x = 0;
	int y = 0;
	int color = 0;

	for (y = 0; y < PLAYFIELD_H; y++) {
		xyprint(PLAYFIELD_XX, PLAYFIELD_Y + y, "");
		for (x = 0; x < PLAYFIELD_W; x++) {
			color = (*(playfield + y) >> (x * 3)) & 7;
			if (color) {
				set_bg(color);
				set_fg(color);
				printf(FILLED_CELL);
				reset_colors();
			}
			else {
				printf(PLAYFIELD_EMPTY_CELL);
			}
		}
	}
}

 

int line_complete(int line) {
    int i = 0;

    for (i = 0; i < PLAYFIELD_W; i++) {
        if (((line >> (i * 3)) & 7) == 0) {
            return 0;
        }
    }
    return 1;
}

int process_complete_lines(int *playfield) { // 완성된 라인 제거 및 갯수 반환
    int i = 0;
    int j = 0;
    int complete_lines = 0;

    for (i = 0; i < PLAYFIELD_H; i++) { 
        if (line_complete(*(playfield + i))) { // 1라인씩 완성된 라인 제거
            for (j = i; j > 0; j--) {
                *(playfield + j) = *(playfield + j - 1);//
            }
            *playfield = 0;
            complete_lines++; // 1라인 완성 당 1증가
        }
    }
    return complete_lines;
}

void update_score(int complete_lines) {
    static int lines_completed = 0;
    static int score = 0;
    static int level = 1;
    char buf[64];

    lines_completed += complete_lines;
    score += (complete_lines * complete_lines);
    if (score > LEVEL_UP * level) {
        tetris_delay *= DELAY_FACTOR;
        level++;
    }
    set_bold();
    set_fg(SCORE_COLOR);
    sprintf(buf, "Lines completed: %d", lines_completed);
    xyprint(SCORE_X, SCORE_Y,     buf);
    sprintf(buf, "Level:           %d", level);
    xyprint(SCORE_X, SCORE_Y + 1, buf);
    sprintf(buf, "Score:           %d", score);
    xyprint(SCORE_X, SCORE_Y + 2, buf);
    reset_colors();
}

void process_fallen_piece(tetris_piece_s *piece, int *playfield, int *playfield1) {
    int complete_lines = 0;
	int i;
	srand(time(NULL));

    flatten_piece(piece, playfield);
    complete_lines = process_complete_lines(playfield);
    if (complete_lines > 0) {
		attack1 = complete_lines;//
        /*update_score(complete_lines);*/
        draw_playfield(playfield);
    }
}

void process_fallen_piece1(tetris_piece_s *piece, int *playfield, int *playfield1) { // 2p blcok 낙하시 액션
	int complete_lines = 0;
	int i;

	flatten_piece(piece, playfield);
	complete_lines = process_complete_lines(playfield); // 반환 라인 갯수
	if (complete_lines > 0) {
		attack2 = complete_lines;// 반환 라인 갯수를 공격 카운트에 삽입
		/*update_score(complete_lines);*/
		draw_playfield1(playfield);// 변경된 player 화면 갱신
	}
}

void cmd_right(tetris_piece_s *piece, int *playfield) {
    move(piece, playfield, 1, 0, 0);
}

void cmd_left(tetris_piece_s *piece, int *playfield) {
    move(piece, playfield, -1, 0, 0);
}

void cmd_rotate(tetris_piece_s *piece, int *playfield) {
    move(piece, playfield, 0, 0, 1);
}

int cmd_down(tetris_piece_s *piece, int *playfield, int *playfield1) {
    if (move(piece, playfield, 0, 1, 0) == 1) {
        return 1;
    }
    process_fallen_piece(piece, playfield, playfield1);
    return 0;
}

int cmd_down1(tetris_piece_s *piece, int *playfield, int *playfield1) { // 2p blcok down에 대한 움직임 설정
	if (move(piece, playfield, 0, 1, 0) == 1) {
		return 1;
	}
	process_fallen_piece1(piece, playfield, playfield1);
	return 0;
}

void cmd_drop(tetris_piece_s *piece, int *playfield, int *playfield1) {
    while (cmd_down(piece, playfield,playfield1)) {
    }
}

void cmd_drop1(tetris_piece_s *piece, int *playfield, int *playfield1) { // 2p block drop에 대한 움직임 설정
	while (cmd_down1(piece, playfield,playfield1)) {
	}
}

void draw_help(int visible) {
    char *text[] = {
		"      Player 1",
        "  Use cursor keys",
        "       or",
        "    r: rotate",
        "d: left,  g: right",
        "    a: drop",
        "      q: quit"
    };
    char spaces[] = "                   ";
    int i = 0;

    if (visible) {
        set_fg(HELP_COLOR);
        set_bold();
    }
    for (i = 0; i < sizeof(text) / sizeof(text[0]); i++) {
        xyprint(HELP_XX, HELP_Y + i, visible ? text[i] : spaces);
    }
    if (visible) {
        reset_colors();
    }
}

void draw_help1(int visible) { // p2 도움말 추가
	char *text[] = {
		"      Player 2",
		"  Use cursor keys",
		"       or",
		"    8: rotate",
		"4: left,  6: right",
		"    p: drop",
		"      q: quit"
	};
	char spaces[] = "                   ";
	int i = 0;

	if (visible) {
		set_fg(HELP_COLOR);
		set_bold();
	}
	for (i = 0; i < sizeof(text) / sizeof(text[0]); i++) {
		xyprint(HELP_XXX, HELP_Y + i, visible ? text[i] : spaces);
	}
	if (visible) {
		reset_colors();
	}
}

void draw_border() {
    int x1 = PLAYFIELD_X - 2;
    int x2 = PLAYFIELD_X + PLAYFIELD_W * 2;
    int i = 0;
    int y = 0;

    set_bold();
    set_fg(BORDER_COLOR);
    for (i = 0; i < PLAYFIELD_H + 1; i++) {
        y = i + PLAYFIELD_Y;
        xyprint(x1, y, "<|");
        xyprint(x2, y, "|>");
    }

    y = PLAYFIELD_Y + PLAYFIELD_H;
    for (i = 0; i < PLAYFIELD_W; i++) {
        x1 = i * 2 + PLAYFIELD_X;
        xyprint(x1, y, "==");
        xyprint(x1, y + 1, "\\/");
    }

    reset_colors();
}

void draw_border1() { // 2p 기본 field 그리기
	int xx1 = PLAYFIELD_XX - 2;
	int xx2 = PLAYFIELD_XX + PLAYFIELD_W * 2;
	int i = 0;
	int y = 0;

	set_bold();
	set_fg(BORDER_COLOR);

	for (i = 0; i < PLAYFIELD_H + 1; i++) {
		y = i + PLAYFIELD_Y;
		xyprint(xx1, y, "<|");
		xyprint(xx2, y, "|>");
	}

	y = PLAYFIELD_Y + PLAYFIELD_H;
	for (i = 0; i < PLAYFIELD_W; i++) {
		xx1 = i * 2 + PLAYFIELD_XX;
		xyprint(xx1, y, "==");
		xyprint(xx1, y + 1, "\\/");
	}

	reset_colors();
}

tetris_piece_s get_next_piece(int visible) {
    static int square_data[] = { 1, 0x1256 };
    static int line_data[] = { 2, 0x159d, 0x4567 };
    static int s_data[] = { 2, 0x4512, 0x0459 };
    static int z_data[] = { 2, 0x0156, 0x1548 };
    static int l_data[] = { 4, 0x159a, 0x8456, 0x0159, 0x2654 };
    static int r_data[] = { 4, 0x1598, 0x0456, 0x2159, 0xa654 };
    static int t_data[] = { 4, 0x1456, 0x1596, 0x4569, 0x4159 };
    static int *piece_data[] = {
        square_data,
        line_data,
        s_data,
        z_data,
        l_data,
        r_data,
        t_data
    };
    static int piece_data_len = sizeof(piece_data) / sizeof(piece_data[0]);
    static int colors[] = { RED, GREEN, YELLOW, BLUE, FUCHSIA, CYAN, WHITE};
    int next_piece_index = random() % piece_data_len;
    int *next_piece_data = piece_data[next_piece_index];
    tetris_piece_s next_piece;

    next_piece.origin_x = NEXT_X;
    next_piece.origin_y = NEXT_Y;
    next_piece.x = 0;
    next_piece.y = 0;
    next_piece.color = colors[random() % (sizeof(colors) / sizeof(colors[0]))];
    next_piece.data = next_piece_data + 1;
    next_piece.symmetry = *next_piece_data;
    next_piece.orientation = random() % next_piece.symmetry;
    strcpy(next_piece.empty_cell, NEXT_EMPTY_CELL);
    draw_piece(next_piece, visible);
    return next_piece;
}

tetris_piece_s get_next_piece1(int visible) { // 2p 플레이어를 위한 next piece
	static int square_data[] = { 1, 0x1256 };
	static int line_data[] = { 2, 0x159d, 0x4567 };
	static int s_data[] = { 2, 0x4512, 0x0459 };
	static int z_data[] = { 2, 0x0156, 0x1548 };
	static int l_data[] = { 4, 0x159a, 0x8456, 0x0159, 0x2654 };
	static int r_data[] = { 4, 0x1598, 0x0456, 0x2159, 0xa654 };
	static int t_data[] = { 4, 0x1456, 0x1596, 0x4569, 0x4159 };
	static int *piece_data[] = {
		square_data,
		line_data,
		s_data,
		z_data,
		l_data,
		r_data,
		t_data
	};
	static int piece_data_len = sizeof(piece_data) / sizeof(piece_data[0]);
	static int colors[] = { RED, GREEN, YELLOW, BLUE, FUCHSIA, CYAN, WHITE };
	int next_piece_index = random() % piece_data_len;
	int *next_piece_data = piece_data[next_piece_index];
	tetris_piece_s next_piece;

	next_piece.origin_x = NEXT_XX;
	next_piece.origin_y = NEXT_Y;
	next_piece.x = 0;
	next_piece.y = 0;
	next_piece.color = colors[random() % (sizeof(colors) / sizeof(colors[0]))];
	next_piece.data = next_piece_data + 1;
	next_piece.symmetry = *next_piece_data;
	next_piece.orientation = random() % next_piece.symmetry;
	strcpy(next_piece.empty_cell, NEXT_EMPTY_CELL);
	draw_piece(next_piece, visible);
	return next_piece;
}

void redraw_screen(int help_visible, tetris_piece_s next_piece, int next_visible, tetris_piece_s current_piece, int *playfield) {
    clear_screen();
    draw_help(help_visible);
    //update_score(0);//
    draw_border();
    draw_playfield(playfield);
    draw_piece(next_piece, next_visible);
    draw_piece(current_piece, 1);
}

// 2p를 위한 초기 종합 스크린구성
void redraw_screen1(int help_visible, tetris_piece_s next_piece, int next_visible, tetris_piece_s current_piece, int *playfield) {
	//clear_screen();//
	draw_help1(help_visible);
	//update_score(0);//
	draw_border1();
	draw_playfield1(playfield);
	draw_piece(next_piece, next_visible);
	draw_piece(current_piece, 1);
}


tetris_piece_s get_current_piece(tetris_piece_s next_piece, int *playfield) {
    tetris_piece_s current_piece = next_piece;
    current_piece.x = (PLAYFIELD_W - 4) / 2;
    current_piece.y = 0;
    current_piece.origin_x = PLAYFIELD_X;
    current_piece.origin_y = PLAYFIELD_Y;
    strcpy(current_piece.empty_cell, PLAYFIELD_EMPTY_CELL);
    if (!position_ok(current_piece, playfield, NULL)) {
        cmd_quit();
    }
    draw_piece(next_piece, 0);
    draw_piece(current_piece, 1);
    return current_piece;
}

tetris_piece_s get_current_piece1(tetris_piece_s next_piece, int *playfield) { // 2p 플레이어를 위한 현재 block
	tetris_piece_s current_piece = next_piece;
	current_piece.x = (PLAYFIELD_W - 4) / 2;
	current_piece.y = 0;
	current_piece.origin_x = PLAYFIELD_XX;
	current_piece.origin_y = PLAYFIELD_Y;
	strcpy(current_piece.empty_cell, PLAYFIELD_EMPTY_CELL);
	if (!position_ok(current_piece, playfield, NULL)) {
		cmd_quit();
	}
	draw_piece(next_piece, 0);
	draw_piece(current_piece, 1);
	return current_piece;
} // 추가

char get_key(long delay) {
    static char buf[16];
    static int buf_len = 0;
    static int buf_pos = 0;
    struct timeval t;
    fd_set fs;

    if (buf_len > 0 && buf_pos < buf_len) {
        return buf[buf_pos++];
    }
    buf_len = 0;
    buf_pos = 0;
    t.tv_sec = 0;
    t.tv_usec = 0;
    if (delay > 0) {
        t.tv_sec = delay / 1000000;
        t.tv_usec = delay % 1000000;
    }
    FD_ZERO(&fs);
    FD_SET(STDIN_FILENO, &fs);
    select(STDIN_FILENO + 1, &fs, 0, 0, &t);

    if (FD_ISSET(STDIN_FILENO, &fs)) {
        buf_len = read(STDIN_FILENO, buf, 16);
        if (buf_len > 0) {
            return buf[buf_pos++];
        }
    }
    return 0;
}

long get_current_micros() {
    struct timeval t;

    gettimeofday(&t, NULL);
    return t.tv_usec + t.tv_sec * 1000000;
}

int main() {
    char c = 0;
    char key[] = {0, 0, 0};
    tcflag_t c_lflag_orig = 0;
    int help_visible = 1;
    int next_visible = 1;
    tetris_piece_s next_piece, next_piece1;// 2p next_piece1
    tetris_piece_s current_piece, current_piece1; // 2p current_piecee1
    int playfield[PLAYFIELD_H] = {};
	int playfield1[PLAYFIELD_H] = {}; // 2p playfield 추가
    int i = 0;
    int flags = fcntl(STDOUT_FILENO, F_GETFL);
    long last_down_time = 0;
    long now = 0;

    fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);
    tcgetattr(STDIN_FILENO, &terminal_conf);
    c_lflag_orig = terminal_conf.c_lflag;
    terminal_conf.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal_conf);
    terminal_conf.c_lflag = c_lflag_orig;

    last_down_time = get_current_micros();
    srandom(time(NULL));
    for (i = 0; i < PLAYFIELD_H; i++) {
        playfield[i] = 0;
    }
    hide_cursor();
    next_piece = get_next_piece(next_visible);
    current_piece = get_current_piece(next_piece, playfield);
    next_piece = get_next_piece(next_visible);

	next_piece1 = get_next_piece1(next_visible);//2p 초기 block 생성
	current_piece1 = get_current_piece1(next_piece1, playfield1);//2p 초기 block 표현
	next_piece1 = get_next_piece1(next_visible);//2p 다음 block 생성

    redraw_screen(help_visible, next_piece, next_visible, current_piece, playfield);
	redraw_screen1(help_visible, next_piece1, next_visible, current_piece1, playfield1);// 추가
    fflush(stdout);
    while(1) {
        now = get_current_micros();
        c = get_key(last_down_time + tetris_delay - now);
        key[2] = key[1];
        key[1] = key[0];
        if (key[2] == ESC && key[1] == '[') {
            key[0] = c;
        } else {
            key[0] = tolower(c);
        }
        switch(key[0]) {
            case 3:
            case 'q':
                cmd_quit();
                break;
            //case 'C'://
            case 'g':
                cmd_right(&current_piece, playfield);
                break;
            //case 'D'://
            case 'd':
                cmd_left(&current_piece, playfield);
                break;
            //case 'A'://
            case 'r':
                cmd_rotate(&current_piece, playfield);
                break;
            case 'f':
                last_down_time = get_current_micros();
                if (!cmd_down(&current_piece, playfield, playfield1)) { // block이 바닥에 도달하면
					while (attack2 > 0) { // player 2의 공격 카운트 감지
						for (i = 0; i < 19; i++) // 한칸씩 위로 shift
							*(playfield + i) = *(playfield + i + 1);
						*(playfield + 19) = 7;
						for (i = 0; i < 9; i++) { // 제일 아랫칸을 꽉 채움
							*(playfield + 19) = (*(playfield + 19) << 3) + 7; // 3bit 당 1조각으로 감지
						}
						*(playfield + 19) = *(playfield + 19) ^ (7 << 3 * ((rand() % 8)));//(rand() % 8) + 1
						// 그 중 랜덤으로 1공간 확보 (XOR)
						attack2--;
					}
					draw_playfield(playfield); // player 화면 갱신


                    current_piece = get_current_piece(next_piece, playfield);
                    next_piece = get_next_piece(next_visible);
                }
                break;
            case 'a':
				cmd_drop(&current_piece, playfield, playfield1);
				while (attack2 > 0) {
					for (i = 0; i < 19; i++)
						*(playfield + i) = *(playfield + i + 1);
					*(playfield + 19) = 7;
					for (i = 0; i < 9; i++) {
						*(playfield + 19) = (*(playfield + 19) << 3) + 7;
					}
					*(playfield + 19) = *(playfield + 19) ^ (7 << 3 * ((rand() % 8)));//(rand() % 8) + 1
					attack2--;
				}
				
				draw_playfield(playfield);
                current_piece = get_current_piece(next_piece, playfield);
                next_piece = get_next_piece(next_visible);
                break;
				//여기부터
				//case 'C'://
			case '6':
				cmd_right(&current_piece1, playfield1);//추가
				break;
				//case 'D'://
			case '4':
				cmd_left(&current_piece1, playfield1);//추가
				break;
				//case 'A'://
			case '8':
				cmd_rotate(&current_piece1, playfield1);//추가
				break;
			case '5':
				last_down_time = get_current_micros();
				if (!cmd_down1(&current_piece1, playfield1,playfield)) {
					while (attack1 > 0) {
						for (i = 0; i < 19; i++)
							*(playfield1 + i) = *(playfield1 + i + 1);
						*(playfield1 + 19) = 7;
						for (i = 0; i < 9; i++) {
							*(playfield1 + 19) = (*(playfield1 + 19) << 3) + 7;
						}
						*(playfield1 + 19) = *(playfield1 + 19) ^ (7 << 3 * ((rand() % 8)));//(rand() % 8) + 1
						attack1--;
					}
					
					draw_playfield1(playfield1);

					current_piece1 = get_current_piece1(next_piece1, playfield1);
					next_piece1 = get_next_piece1(next_visible);

				}
				break;
			case 'p':
				cmd_drop1(&current_piece1, playfield1, playfield);
				while (attack1 > 0) {
					for (i = 0; i < 19; i++)
						*(playfield1 + i) = *(playfield1 + i + 1);
					*(playfield1 + 19) = 7;
					for (i = 0; i < 9; i++) {
						*(playfield1 + 19) = (*(playfield1 + 19) << 3) + 7;
					}
					*(playfield1 + 19) = *(playfield1 + 19) ^ (7 << 3 * ((rand() % 8)));//(rand() % 8) + 1
					attack1--;
				}
				draw_playfield1(playfield1);

				current_piece1 = get_current_piece1(next_piece1, playfield1);
				next_piece1 = get_next_piece1(next_visible);

				break;
				
			case 0:
				last_down_time = get_current_micros();
				if (!cmd_down(&current_piece, playfield,playfield1)) {
					while (attack2 > 0) {
						for (i = 0; i < 19; i++)
							*(playfield + i) = *(playfield + i + 1);
						*(playfield + 19) = 7;
						for (i = 0; i < 9; i++) {
							*(playfield + 19) = (*(playfield + 19) << 3) + 7;
						}
						*(playfield + 19) = *(playfield + 19) ^ (7 << 3 * ((rand() % 8)));//(rand() % 8) + 1
						attack2--;
					}

					draw_playfield(playfield);
					current_piece = get_current_piece(next_piece, playfield);
					next_piece = get_next_piece(next_visible);
				}
				if (!cmd_down1(&current_piece1, playfield1,playfield)) {
					while (attack1 > 0) {
						for (i = 0; i < 19; i++)
							*(playfield1 + i) = *(playfield1 + i + 1);
						*(playfield1 + 19) = 7;
						for (i = 0; i < 9; i++) {
							*(playfield1 + 19) = (*(playfield1 + 19) << 3) + 7;
						}
						*(playfield1 + 19) = *(playfield1 + 19) ^ (7 << 3 * ((rand() % 8)));//(rand() % 8) + 1
						attack1--;
					}

					draw_playfield1(playfield1);
					current_piece1 = get_current_piece1(next_piece1, playfield1);
					next_piece1 = get_next_piece1(next_visible);
				}
				break;

            case 'h':
                help_visible ^= 1;
                draw_help(help_visible);
                break;
            case 'n':
                next_visible ^= 1;
                draw_piece(next_piece, next_visible);
                break;
            case 'c':
                use_color ^= 1;
                redraw_screen(help_visible, next_piece, next_visible, current_piece, playfield);
                break;
            default:
                break;
        }
        fflush(stdout);
    }
}
