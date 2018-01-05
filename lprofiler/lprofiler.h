#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#include<string>
#include<map>
#include<vector>

struct profileInfo;


typedef long long int64;
typedef std::map<std::string, profileInfo*> pmap;
typedef std::vector<profileInfo*> pvec;
typedef std::map<std::string, bool> mapChildIdstr;

struct profileInfo {
	std::string func_idstr;
	int64 count;
	double time;
	bool prevent;
	mapChildIdstr mapChildIds;

	profileInfo(std::string& str): 
		func_idstr(str),
		count(0),
		time(0),
		prevent(false) {

	}
};

LUAMOD_API int luaopen_lprofiler(lua_State *L);

#	include<time.h>
#ifdef _WIN32_
#   include <windows.h>
int gettimeofday(struct timeval *tv, void *tzp);
#else
#   include <sys/time.h>
#endif
