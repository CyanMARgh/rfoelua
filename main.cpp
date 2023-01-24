#include <cstdio>
#include <cstdlib>
#include <lua.hpp>
#include <fstream>

int set_active_function(lua_State* L, const char* name) {
	lua_getglobal(L, name);
	if(!lua_isfunction(L, -1)) {
		printf("can't find function: %s\n", name);
		return 1;
	}
	return 0;
}
void dumpstack (lua_State *L) {
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
bool call(lua_State* L, double x, double y) {
	int num_args = 2, num_returns = 1;
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_pcall(L, num_args, num_returns, 0);
	int result = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return result;
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
		return 1;
	}
	return 0;
}
int main() {	
	const int X = 90, Y = 30;
	const float s = 1.f;

	lua_State* L = luaL_newstate();
	run_file(L, "hello.lua");
	for(int y = 0; y < Y; y++) {
		float fy = (double)y / (Y - 1);
		fy = (fy - .5) * 2.f * s;
		for(int x = 0; x < X; x++) {
			float fx = (double)x / (X - 1);
			fx = (fx - .5) * ((double)X / Y) * s;

			set_active_function(L, "painter");
			bool res = call(L, fx, fy);
			putchar(res ? '#' : ' ');
		}
		puts("");
	}
	return 0;
}