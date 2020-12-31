local M = {}

local n = 0

local used_colors = {}

function M.set_n(_n)
	n = _n
end

function M.remove_color(x, y, z)
	for i = #used_colors, 1, -1 do
		if used_colors[1] == x and used_colors[y] == y and used_colors[z] == z then
			table.remove(used_colors, i)
		end
	end
end

-- Not the best solution, but it will do. Each province must have a unique color - this is the main thing
function M.ground()
	local x = math.floor(lume.random(0, 255))
	local y = math.floor(lume.random(0, 255))
	local z = math.floor(lume.random(0, 128))

	while #used_colors < 5400 and lume.match(used_colors, function(v)
		local dif = math.abs(v[1] - x) + math.abs(v[2] - y) + math.abs(v[3] - z)
		return dif < 12
	end) do
		x = math.floor(lume.random(0, 255))
		y = math.floor(lume.random(0, 255))
		z = math.floor(lume.random(0, 128))
	end

	local t = {
		x, y, z
	}

	table.insert(used_colors, t)

	return color_to_vector(t)
end

function M.water()
	local x = math.floor(lume.random(0, 64))
	local y = math.floor(lume.random(0, 255))
	local z = math.floor(lume.random(225, 255))

	while #used_colors < 5400 and lume.match(used_colors, function(v)
		local dif = math.abs(v[1] - x) + math.abs(v[2] - y) + math.abs(v[3] - z)
		return dif < 12
	end) do
		x = math.floor(lume.random(0, 64))
		y = math.floor(lume.random(0, 255))
		z = math.floor(lume.random(225, 255))
	end

	local t = {
		x, y, z
	}

	table.insert(used_colors, t)

	return color_to_vector(t)
end

return M