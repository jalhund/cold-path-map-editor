local M = {}

M.provinces = {}
local provinces_data = {}

local check_click = require "scripts.check_click"
local positions_in_atlas = require "go.positions_in_atlas"

function M.init(self, map_data)
	provinces = {}
	local file
	local province_info
	local blurred_data
	local generated_data
	local pos
	local size
	local data
	local pos
	check_click.clear()
	for i = 1, map_data.num_of_provinces do
		file = io.open(IMAGE_DATA_PATH.."exported_map/description/"..i, "r")
		data = file:read("*a")
		provinces_data[i] = json.decode(data)
		pprint("Province data:", provinces_data[i])
		pos = vmath.vector3(provinces_data[i].position[1],
			provinces_data[i].position[2], 0)
		M.provinces[i] = factory.create("/factories#province_factory", pos)
		go.set(msg.url(nil, M.provinces[i],"sprite"), "material", self.distancefield)
		print("Add province: ", i)
		check_click.load_from_file(tostring(i), IMAGE_DATA_PATH.."exported_map/generated_data/"..i, provinces_data[i].size[1]
			* provinces_data[i].size[2])
	end
end

function M.late_init(self, map_data)
	local file
	local data
	header = {
		width = 4096,
		height = 4096,
		type = resource.TEXTURE_TYPE_2D,
		format = resource.TEXTURE_FORMAT_LUMINANCE,
		num_mip_maps = 1
	}

	local used = {
		[124] = 0,
		[252] = 0,
		[508] = 0,
		[1020] = 0,
		[2044] = 0
	}

	local buffers = {
		[124] = buffer.create(4096*4096, { {name=hash("luminance"), type=buffer.VALUE_TYPE_UINT8, count=1} } ),
		[252] = buffer.create(4096*4096, { {name=hash("luminance"), type=buffer.VALUE_TYPE_UINT8, count=1} } ),
		[508] = buffer.create(4096*4096, { {name=hash("luminance"), type=buffer.VALUE_TYPE_UINT8, count=1} } ),
		[1020] = buffer.create(4096*4096, { {name=hash("luminance"), type=buffer.VALUE_TYPE_UINT8, count=1} } ),
		[2044] = buffer.create(4096*4096, { {name=hash("luminance"), type=buffer.VALUE_TYPE_UINT8, count=1} } )
	}

	local resource_path
	for i = 1, map_data.num_of_provinces do
		-- print("Province: ", provinces_data[i].size[1], provinces_data[i].size[2])
		local size = provinces_data[i].size[1]
		used[size] = used[size] + 1
		drawpixels.get_file_data(buffers[size], IMAGE_DATA_PATH.."exported_map/blurred_data/"..i,
			 provinces_data[i].size[1], provinces_data[i].size[2], size, used[size])

		sprite.play_flipbook(M.provinces[i], tostring(positions_in_atlas[size][used[size]]))
		go.set(msg.url(nil, M.provinces[i],"sprite"), "image", self["atlas_"..size])
		resource_path = go.get(msg.url(nil, M.provinces[i],"sprite"), "texture0")
		resource.set_texture(resource_path, header, buffers[size])
	end
end

function M.get(id)
	return provinces[id]
end

return M