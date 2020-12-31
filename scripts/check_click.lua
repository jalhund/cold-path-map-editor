local M = {}

local perfomance = require "scripts.sarah.perfomance"

local data_compression = {}

--Modified: https://github.com/dapetcu21/crit/blob/master/crit/pick.lua
local h_size = hash("size")

local function pick_sprite(game_object, x, y)
	local url = msg.url(nil, game_object, "sprite")
	local size = go.get(url, h_size)
  	local transform = go.get_world_transform(url)
	local pos = vmath.inv(transform) * vmath.vector4(x, y, 0, 1)
	x, y = pos.x, pos.y
	
 	local half_width = size.x * 0.5
	local left = -half_width
	local right = half_width
	if x < left or x > right then return false end
	
 	local half_height = size.y * 0.5
	local top = half_height
	local bottom = -half_height
	if y < bottom or y > top then return false end
	
 	return true
end

local function check_transparency(game_object, x, y, id)
	-- print("Check click: game_object", game_object)
	local url = msg.url(nil, game_object, "sprite")
	local size = go.get(url, h_size)
  	local transform = go.get_world_transform(url)
	local pos = vmath.inv(transform) * vmath.vector4(x, y, 0, 1)
	x, y = pos.x, pos.y
	local compression = 1 --data_compression[game_object]
 	x = math.floor(x + size.x * 0.5) + 1
	y = math.floor(y + size.y * 0.5) + 1
	local i = math.floor(y/compression)*math.floor(size.x/compression) + math.floor(x/compression + 0.5)

	return checkclick.checkaplha(id, i)
end

function M.clear()
	checkclick.clear()
end

function M.load_from_file(i, path, size)
	checkclick.load_from_file(i, path, size)
end

function M.init(init_list)
	local s_t = socket.gettime()

	local string_byte = string.byte
	local string_gmatch = string.gmatch
	local sys_load_resource = sys.load_resource
	local zlib_inflate = zlib.inflate
	local pairs = pairs

	local str_begin = "/assets/generated_data/"..game_data.map.."/"

	checkclick.clear()
	data_compression = {}

	for k, v in pairs(init_list) do
		local loaded_data = sys_load_resource(str_begin..v..".data")
		if loaded_data then
			loaded_data = zlib_inflate(loaded_data)
			data_compression[v] = checkclick.init(v, loaded_data)
		else
			print("Error data not loaded:" , v)
		end
	end

	-- print("Finished total load: ", socket.gettime() - s_t)
end

function M.check(game_object, x, y, id)
	if pick_sprite(game_object, x, y) and check_transparency(game_object, x, y, id) then
		return true
	else
		return false
	end
end

return M