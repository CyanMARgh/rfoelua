#include <cstdio>
#include <cstdlib>
#include <lua.hpp>
#include <fstream>
#include <ncurses.h>
#include <vector>
#include <string>

typedef uint8_t u8;
typedef int32_t s32;
typedef uint32_t u32;

struct ivec2 {
	s32 x = 0, y = 0;
};
ivec2 operator+(ivec2 a, ivec2 b) {
	return {a.x + b.x, a.y + b.y};
}
ivec2& operator+=(ivec2& a, ivec2 b) {
	a.x += b.x, a.y += b.y;
	return a;
}
ivec2 operator-(ivec2 a, ivec2 b) {
	return {a.x - b.x, a.y - b.y};
}
ivec2& operator-=(ivec2& a, ivec2 b) {
	a.x -= b.x, a.y -= b.y;
	return a;
}

int set_active_function(lua_State* L, const char* name) {
	lua_getglobal(L, name);
	if(!lua_isfunction(L, -1)) {
		printf("can't find function: %s\n", name);
		return 1;
	}
	return 0;
}
void dumpstack(lua_State *L) {
	int top = lua_gettop(L);
	printf("stack size: %d\n", top);
	for (int i = 1; i <= top; i++) {
		printf("id = %2d, type = %8s, value = ", i, luaL_typename(L,i));
		switch (lua_type(L, i)) {
			case LUA_TNUMBER:
				printf("%g\n",lua_tonumber(L,i));
				break;
			case LUA_TSTRING:
				printf("%s\n",lua_tostring(L,i));
				break;
			case LUA_TBOOLEAN:
				printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
				break;
			case LUA_TNIL:
				printf("%s\n", "nil");
				break;
			default:
				printf("%p\n",lua_topointer(L,i));
				break;
		}
	}
	puts("===================");
}
std::string read_entire_file(const std::string& path) {
	std::ifstream file(path);
	if(!file) {
		printf("failed to open file: %s\n", path.c_str());
		exit(-1);
	}
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return content;
}
int run_file(lua_State* L, const std::string& path) {
	std::string src = read_entire_file(path);
	int result = luaL_dostring(L, src.c_str());
	if(result) {
		printf("error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		exit(-1);
		return 1;
	}
	return 0;
}
double call(lua_State* L, const char* fname, int sx, int sy, int x, int y) {
	int num_args = 4, num_returns = 1;
	set_active_function(L, fname);
	lua_pushnumber(L, sx);
	lua_pushnumber(L, sy);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_pcall(L, num_args, num_returns, 0);
	double result = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return result;
}

void init_ncurses() {
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	set_escdelay(25);

	if(!has_colors()) {
		exit(-1);
	}
	start_color();

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_WHITE, COLOR_GREEN);
}
void finalize_ncurses() {
	endwin();
}
const int MAX_BRIGHTNESS = 8;
char brightness_map[MAX_BRIGHTNESS + 1] = " .,:ilwW";
struct Map {
	ivec2 size;
	std::vector<float> map;

	void draw_inside(WINDOW* win) {
		for(int y = 0, i = 0; y < size.y; y++) {
			wmove(win, y + 1, 1);
			for(int x = 0; x < size.x; x++, i++) {
				int b = (int)(map[i] * MAX_BRIGHTNESS);
				b = b < 0 ? MAX_BRIGHTNESS : b >= MAX_BRIGHTNESS ? MAX_BRIGHTNESS - 1 : b;
				waddch(win, brightness_map[b]);
			}
		}
	}
};
int min(int x, int y) {
	return x < y ? x : y;
}
int max(int x, int y) {
	return x > y ? x : y;
}
void ncexit(int code, const std::string& info) {
	finalize_ncurses();
	if(code) printf("%s\n", info.c_str());
	exit(code);
}

struct App {
	ivec2 size_canvas, size_map_max, size_map;
	int line_id = 0, line_id_max = 2;
	WINDOW *map_win, *select_win, *debug_win;
	lua_State* L;

	void init() {
		L = luaL_newstate();
		run_file(L, "hello.lua");

		getmaxyx(stdscr, size_canvas.y, size_canvas.x);
		size_map_max = size_canvas - ivec2{9, 2};
		if(size_map_max.x < 5 || size_map_max.y < 5) {
			ncexit(-1, "canvas too small: " + std::to_string(size_map_max.x) + " " + std::to_string(size_map_max.y));
		}
		size_map = ivec2{min(size_map_max.x, 40), min(size_map_max.y, 20)};

		map_win = newwin(size_map_max.y + 2, size_map_max.x + 2, 0, 0);
		select_win = newwin(4, 7, 0, size_map_max.x + 2);
		debug_win = newwin(7, 7, 4, size_map_max.x + 2);

		box(select_win, 0, 0);
		box(debug_win, 0, 0);

		keypad(select_win, true);		
		draw(true, true, true);
	}
	void draw(bool map, bool select, bool debug = false) {
		if(map) {
			wclear(map_win);
			box(map_win, 0, 0);
			for(int y = 0; y < size_map.y; y++) {
				wmove(map_win, 1 + y, 1);
				for(int x = 0; x < size_map.x; x++) {
					double res = call(L, "painter", size_map.x, size_map.y, x, y);
					waddch(map_win, res > .5 ? '#' : ' ');
				}
			}
			wrefresh(map_win);
		}
		if(select) {
			if(line_id == 0) { wattron(select_win, A_REVERSE); } else { wattroff(select_win, A_REVERSE); }
			mvwprintw(select_win, 1, 1, "%c%3d%c", size_map.x == 1 ? ' ' : '<', size_map.x, size_map.x == size_map_max.x ? ' ' : '>');
			if(line_id == 1) { wattron(select_win, A_REVERSE); } else { wattroff(select_win, A_REVERSE); }
			mvwprintw(select_win, 2, 1, "%c%3d%c", size_map.y == 1 ? ' ' : '<', size_map.y, size_map.y == size_map_max.y ? ' ' : '>');
			wrefresh(select_win);			
		}
		if(debug) {
			wrefresh(debug_win);
		}
	}
	void cycle() {
		while(1) {
			int key = wgetch(select_win);
			mvwprintw(debug_win, 1, 1, "%5d", key);
			mvwprintw(debug_win, 2, 1, "%5d", KEY_DOWN);
			draw(false, false, true);

			int r = key_process(key);
			if(r == 2) {
				run_file(L, "hello.lua");
				draw(true, false, false);
			} else if(r == 3) {
				draw(false, true, false);
			} else if(r == 1) {
				return;
			}
		}
	}
	void finalize() {
		delwin(map_win);
		delwin(select_win);
		delwin(debug_win);
	}
	void full_cycle() {
		init();
		cycle();
		finalize();
	}
	int key_process(int key) {
		if(key == 'R' || key == 'r') {
			return 2;
		} else if(key == KEY_DOWN) {
			line_id = min(line_id + 1, line_id_max - 1);
			return 3;
		} else if(key == KEY_UP) {
			line_id = max(line_id - 1, 0);
			return 3;
		} else if(key == KEY_RIGHT) {
			if(line_id == 0) {
				size_map.x = min(size_map.x + 1, size_map_max.x);
			} else {
				size_map.y = min(size_map.y + 1, size_map_max.y);					
			}
			return 3;
		} else if(key == KEY_LEFT) {
			if(line_id == 0) {
				size_map.x = max(size_map.x - 1, 1);
			} else {
				size_map.y = max(size_map.y - 1, 1);					
			}
			return 3;
		} else if(key == 27) {
			return 1;
		}
		return 0;
	}
};

int main() {
	init_ncurses();

	App app;
	app.full_cycle();

	// exit(-2);

	ncexit(0, "");
}