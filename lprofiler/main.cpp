#include "lprofiler.h"

lua_State* L;

int main() {
	L = luaL_newstate();
	luaL_openlibs(L);
	lua_pushcfunction(L, luaopen_lprofiler);
	lua_setglobal(L, "_open_lprofiler");

	const char* entryFile = "test.lua";
	luaL_loadfile(L, entryFile);
	int ret = 0;
	if ((ret = lua_pcall(L, 0, 0, 0))) {
		printf("[luaEntry] errorCode: %d,%s error: %s\n", ret, entryFile, lua_tostring(L, -1));
		lua_pop(L, 1);
		exit(1);
	}
	lua_close(L);
	getchar();
	return 0;
}