#include "lprofiler.h"
#include<map>
#include<time.h>
#include<assert.h>
#include<algorithm>
#include<functional>
#ifdef _WIN32
#include<direct.h>
#include <windows.h>
#endif
#include<string.h>


#ifdef _WIN32
int gettimeofday(struct timeval *tv, void *tzp)
{
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;

	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tv->tv_sec = (long)clock;
	tv->tv_usec = wtm.wMilliseconds * 1000;

	return (0);
}
#endif 

bool is_running = false;
pmap g_allm;
pvec g_allv;

//prevent checking these functions
typedef std::map<const char*, bool> mapFuncName;
mapFuncName mapNames;

void prevent_funcs() {
	const char* f[] = {
		"_prof_start",
		"_prof_stop",
		"_prof_dump",
	};
	for (size_t i = 0; i < sizeof(f) / sizeof(const char*); i++) {
		mapNames[ f[i] ] = true;
	}
}

bool checkPrevent(const char* funcName) {
	printf("[checkPrevent]: %s\n",funcName);
	for (auto it = mapNames.begin(); it != mapNames.end(); it++) {
		if (strcmp(it->first, funcName) == 0) {
			return true;
		}
	}
	return false;
}

void saveRecord(profileInfo* info) {
	auto it = g_allm.find(info->func_idstr);
	if (it == g_allm.end()) {
		profileInfo* newp = new profileInfo(*info);
		g_allm[newp->func_idstr] = newp;
		return;
	}
	it->second->count += info->count;
	it->second->time += info->time;
}

double getSec() {
	timeval t;
	gettimeofday(&t, NULL);
	return (t.tv_sec + double(t.tv_usec / 1000000.0));
}

bool cmp_count(profileInfo* obj1, profileInfo* obj2) {
	return obj1->count > obj2->count;
}

bool cmp_time(profileInfo* obj1, profileInfo* obj2) {
	return obj1->time > obj2->time;
}

bool cmp_avg(profileInfo* obj1, profileInfo* obj2) {
	return obj1->timePerCall > obj2->timePerCall;
}

//event: LUA_HOOKCALL, LUA_HOOKRET, LUA_HOOKTAILCALL, LUA_HOOKLINE, and LUA_HOOKCOUNT
static void profile_hook(lua_State *L, lua_Debug *ar) {
	lua_Debug previous_ar;
	if (lua_getstack(L, 1, &previous_ar) == 0) { //no caller info
		return;
	}

	bool is_prevent = false;
	lua_getinfo(L, "nSl", ar); //current call info
	//:TODO:去掉过滤
	//if (ar->name && checkPrevent(ar->name)) { //this call is to prevent checking
	//	is_prevent = true;
	//}

	profileInfo* lastest = nullptr;
	if (g_allv.size() > 0) {
		lastest = g_allv.back();
		//:TODO:去掉过滤
		//if (lastest->prevent) {
		//	is_prevent = true;
		//}
	}

	if (ar->event == LUA_HOOKCALL) {
		//if (ar->name) {
		//	printf("call: %s\n", ar->name);
		//}
		//else {
		//	printf("call: unknow\n");
		//}

		//ukey
		char idstr[2048] = { '\0' };
		snprintf(idstr, sizeof(idstr), "%s:%s:%d:%s",
			ar->what != NULL ? ar->what : "nil",
			ar->source != NULL ? ar->source : "nil",
			ar->currentline > 0 ? ar->currentline : -1,
			ar->name != NULL ? ar->name : "nil");
		std::string str(idstr);
	
		profileInfo* curInfo = new profileInfo(str);
		// just init data when called
		curInfo->prevent = is_prevent;
		curInfo->time = getSec();
		curInfo->count++;
		g_allv.push_back(curInfo); // this is a copy

		if (lastest) {
			auto it = lastest->mapChildIds.find(str);
			if (it == lastest->mapChildIds.end()) {
				lastest->mapChildIds[str] = true;
			}
		}
	}
	else if (ar->event == LUA_HOOKRET) { // calculate time spent
		//if (ar->name) {
		//	printf("return: %s\n", ar->name);
		//}
		//else {
		//	printf("return: unknow\n");
		//}
		profileInfo* curInfo = lastest;
		if (!curInfo) { //this is a return hook, the lastest node must exist
			return;
		}
		curInfo->time = getSec() - curInfo->time;
		//:TODO:去掉过滤
		//if (!curInfo->prevent) {
		//	saveRecord(curInfo);
		//}
		saveRecord(curInfo);
		//'return' release call info
		g_allv.pop_back();
		delete curInfo;
		curInfo = nullptr;
		lastest = nullptr;
	}
}

void clear_allm() {
	for (auto it = g_allm.begin(); it != g_allm.end(); it++) {
		delete it->second;
	}
	g_allm.clear();
}

void clear_allv() {
	for (auto it = g_allv.begin(); it != g_allv.end(); it++) {
		delete *it;
	}
	g_allv.clear();
}

static int profile_start(lua_State* L) {
	if (is_running) return 0;
	clear_allm();
	clear_allv();
	is_running = true;
	lua_sethook(L, profile_hook, LUA_MASKCALL | LUA_MASKRET, 0);
	return 0;
}

static int profile_stop(lua_State* L) {
	clear_allv();
	lua_sethook(L, NULL, 0, 0);
	is_running = false;
	return 0;
}

static int profile_cleanup(lua_State* L) {
	if (is_running) return 0;
	clear_allm();
	clear_allv();
	is_running = false;
	return 0;
}

static int profile_dump(lua_State* L) {
	if (is_running) return 0;

	const char* fpath = lua_tostring(L, 1);
	const char* dumpType = lua_tostring(L, 2);
	if (!dumpType) {
		printf("dump type : count or time or avg ?\n");
		return 0;
	}
	int t = 0;
	if (strcmp(dumpType, "count") == 0) {
		t = 1;
	}
	else if (strcmp(dumpType, "time") == 0) {
		t = 2;
	}
	else if (strcmp(dumpType, "avg") == 0) {
		t = 3;
	} else {
		printf("dump type : count or time or avg ?\n");
		return 0;
	}

	FILE* fh = fopen(fpath, "w");
	if (!fh) return 0;
	std::vector<profileInfo*> vecp;
	for (auto it = g_allm.begin(); it != g_allm.end(); it++) {
		vecp.push_back(it->second);
		if (it->second->count > 0) {
			it->second->timePerCall = it->second->time / it->second->count;
		}
	}
	clear_allv();

	//sort by time spent or count
	if (t == 1) {
		std::sort(vecp.begin(), vecp.end(), cmp_count);
	}
	else if (t == 2) {
		std::sort(vecp.begin(), vecp.end(), cmp_time);
	}
	else {
		std::sort(vecp.begin(), vecp.end(), cmp_avg);
	}

	fprintf(fh, "***************************** profile info: sort type: %s *****************************\n\n",dumpType);
	profileInfo* p = nullptr;
	for (auto it = vecp.begin(); it != vecp.end(); it++) {
		p = *it;
		fprintf(fh, "============================ %s ============================\n", p->func_idstr.c_str());
		if (t == 1) {
			fprintf(fh, "count: %lld\n", p->count);
			fprintf(fh, "time: %f\n", p->time);
			fprintf(fh, "timePerCall: %f\n", p->timePerCall);
		}
		else if (t == 2) {
			fprintf(fh, "time: %f\n", p->time);
			fprintf(fh, "count: %lld\n", p->count);
			fprintf(fh, "timePerCall: %f\n", p->timePerCall);
		}
		else {
			fprintf(fh, "timePerCall: %f\n", p->timePerCall);
			fprintf(fh, "time: %f\n", p->time);
			fprintf(fh, "count: %lld\n", p->count);
		}
		if (p->mapChildIds.size() > 0) {
			fprintf(fh, "childs:\n");
			for (auto it2 = p->mapChildIds.begin(); it2 != p->mapChildIds.end(); it2++) {
				fprintf(fh, "\t%s\n",it2->first.c_str());
			}
		}
	}

	fclose(fh);
	return 0;
}

int luaopen_lprofiler(lua_State *L) {
	luaL_Reg l[] = {
		{ "_prof_start",	profile_start	},
		{ "_prof_stop",		profile_stop	},
		{ "_prof_dump",		profile_dump	},
		{ "_prof_cleanup",	profile_cleanup },
		{ NULL,				NULL			},
	};
	luaL_newlib(L, l);
	prevent_funcs();
	return 1;
}
