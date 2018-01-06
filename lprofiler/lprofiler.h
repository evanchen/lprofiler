#pragma once
#include "lua.hpp"
#include<string>
#include<map>
#include<vector>
#include<time.h>
#ifdef _WIN32
#include <windows.h>
int gettimeofday(struct timeval *tv, void *tzp);
#else
#include <sys/time.h>
#endif

typedef long long int64;

struct profileInfo;


typedef std::map<std::string, profileInfo*> pmap;
typedef std::vector<profileInfo*> pvec;
typedef std::map<std::string, bool> mapChildIdstr;

struct profileInfo {
	std::string func_idstr;
	int64 count;
	double time;
	double timePerCall;
	bool prevent;
	mapChildIdstr mapChildIds;

	profileInfo(std::string& str): 
		func_idstr(str),
		count(0),
		time(0),
		timePerCall(0),
		prevent(false) {

	}
};

int luaopen_lprofiler(lua_State *L);