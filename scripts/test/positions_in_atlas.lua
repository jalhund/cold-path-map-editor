local M = {}

local positions_in_atlas = require "go.positions_in_atlas"

local function test_table(t, n)
	for i = 1, n do
		assert(lume.match(t, function(x) return x == i end), "Error! Not found "..i)
	end
end

local function find_repeat_elements(t)
	local r = {}
	for k, v in pairs(t) do
		r[v] = r[v] and r[v] + 1 or 1
		if r[v] > 1 then
			print("Error: ", v, k, math.floor(k/32) + 1, k % 32 + 1)
		end
	end
end

function M.start()
	test_table(positions_in_atlas[124], 1024)
	find_repeat_elements(positions_in_atlas[124])

	test_table(positions_in_atlas[252], 256)
	find_repeat_elements(positions_in_atlas[252])

	test_table(positions_in_atlas[508], 64)
	find_repeat_elements(positions_in_atlas[508])

	test_table(positions_in_atlas[1020], 16)
	find_repeat_elements(positions_in_atlas[1020])

	test_table(positions_in_atlas[2044], 4)
	find_repeat_elements(positions_in_atlas[2044])
end

return M