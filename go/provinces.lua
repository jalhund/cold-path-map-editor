local M = {}

local inspect = require "scripts.inspect"
-- To convert custom maps to in-game format
local GENERATE_DATA_FOR_IN_GAME_MAP = false

M.provinces = {}
M.provinces_data = {}

local check_click = require "scripts.check_click"

local function save_table_to_file(file_name, t)
    local file = io.open(file_name, "w")
    if not file then
        error("Error open file: "..file_name)
    end
    file:write("local t = "..inspect(t).."\nreturn t")
    file:close()
end

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

    local list_of_water_provinces = {}
    local provinces_positions = {}
    local provinces_offset = {}

	for i = 1, map_data.num_of_provinces do
		file = io.open(IMAGE_DATA_PATH.."exported_map/description/"..i, "r")
		data = file:read("*a")
		M.provinces_data[i] = json.decode(data)
		pprint("Province data:", M.provinces_data[i])
		pos = vmath.vector3(M.provinces_data[i].position[1],
			M.provinces_data[i].position[2], 0)
		M.provinces[i] = factory.create("/factories#province_factory", pos)
		go.set(msg.url(nil, M.provinces[i],"sprite"), "material", self.distancefield)
		print("Add province: ", i)
		check_click.load_from_file(tostring(i), IMAGE_DATA_PATH.."exported_map/generated_data/"..i, M.provinces_data[i].size[1]
			* M.provinces_data[i].size[2])

        if GENERATE_DATA_FOR_IN_GAME_MAP then
            table.insert(list_of_water_provinces, M.provinces_data[i].water)
            table.insert(provinces_positions, M.provinces_data[i].position)
        end
	end

    if GENERATE_DATA_FOR_IN_GAME_MAP then
        save_table_to_file("water_provinces.lua", list_of_water_provinces)
        save_table_to_file("provinces_positions.lua", provinces_positions)
    end
end

function M.late_init(self, map_data)
	for i = 1, map_data.num_of_provinces do
		local province_width = M.provinces_data[i].size[1]
		local province_height = M.provinces_data[i].size[2]

		local buf = buffer.create(province_width * province_height,
			{ { name = hash("luminance"), type = buffer.VALUE_TYPE_UINT8, count = 1 } })
		drawpixels.decompress_lzs_data(buf,
			IMAGE_DATA_PATH .. "exported_map/blurred_data/" .. i,
			province_width, province_height)

		local texture_path = "/dynamic_province_texture_" .. i .. ".texturec"
		local texture_id = resource.create_texture(texture_path, {
			width = province_width,
			height = province_height,
			type = resource.TEXTURE_TYPE_2D,
			format = resource.TEXTURE_FORMAT_LUMINANCE,
			num_mip_maps = 1
		})
		resource.set_texture(texture_id, {
			width = province_width,
			height = province_height,
			x = 0, y = 0,
			type = resource.TEXTURE_TYPE_2D,
			format = resource.TEXTURE_FORMAT_LUMINANCE,
			num_mip_maps = 1
		}, buf)

		local atlas_id = resource.create_atlas(
			"/dynamic_province_atlas_" .. i .. ".texturesetc", {
			texture = texture_id,
			animations = { {
				id = "province_" .. i,
				width = province_width,
				height = province_height,
				frame_start = 1, frame_end = 2,
			} },
			geometries = { {
				width = province_width,
				height = province_height,
				pivot_x = 0.5, pivot_y = 0.5,
				vertices  = { 0, province_height, 0, 0, province_width, 0, province_width, province_height },
				uvs       = { 0, province_height, 0, 0, province_width, 0, province_width, province_height },
				indices   = { 0, 1, 2, 0, 2, 3 }
			} }
		})

		go.set(msg.url(nil, M.provinces[i], "sprite"), "image", atlas_id)
		sprite.play_flipbook(M.provinces[i], "province_" .. i)
	end
end

function M.get(id)
	return provinces[id]
end

return M