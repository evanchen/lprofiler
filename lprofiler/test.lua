
function test_func0(x,y)
	print("11111",x,y)
end 

function test_func1(a,b)
	test_func0(a * 10,b + 2)
end 

function test_func2(c,d)
	test_func1(c,d)
	test_func0(c*2,d*2)
end 

local function test_func3(e,f,g)
	local x = e + f
	local y = f + g
	print("2222",x,y)
	return x,y
end 

function test_func4()
	local ret1,ret2 = test_func3(10,2,3)
	test_func2(ret1,ret2)
end 

local lprof = _open_lprofiler()
assert(lprof)

lprof._prof_start()

test_func4()
test_func2(11,22)

lprof._prof_stop()
lprof._prof_dump("time")
