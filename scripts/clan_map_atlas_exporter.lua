local M = {}

local json = require "scripts.json"
local province_binary = require "scripts.province_binary"
local exported_map_format = require "scripts.exported_map_format"

local function normalize_path(path)
	return string.gsub(tostring(path or ""), "\\", "/")
end

local function ensure_directory(path)
	if not path or path == "" then
		return true
	end
	path = normalize_path(path)
	local attributes = lfs.attributes(path)
	if attributes and attributes.mode == "directory" then
		return true
	end

	local parent = path:match("^(.*)/[^/]+$")
	if parent and parent ~= path then
		local ok, err = ensure_directory(parent)
		if not ok then
			return nil, err
		end
	end

	local ok, err = lfs.mkdir(path)
	if ok or lfs.attributes(path, "mode") == "directory" then
		return true
	end
	return nil, err or ("Error create directory: " .. path)
end

local function remove_path(path)
	path = normalize_path(path)
	local attributes = lfs.attributes(path)
	if not attributes then
		return true
	end
	if attributes.mode ~= "directory" then
		local ok, err = os.remove(path)
		if not ok then
			return nil, err or ("Error remove file: " .. path)
		end
		return true
	end

	for entry in lfs.dir(path) do
		if entry ~= "." and entry ~= ".." then
			local ok, err = remove_path(path .. "/" .. entry)
			if not ok then
				return nil, err
			end
		end
	end

	local ok, err = lfs.rmdir(path)
	if not ok then
		return nil, err or ("Error remove directory: " .. path)
	end
	return true
end

local function read_json_file(path)
	local file = io.open(path, "r")
	if not file then
		return nil, "Error open file: " .. path
	end
	local data = file:read("*a")
	file:close()

	local ok, decoded = pcall(json.decode, data)
	if not ok or type(decoded) ~= "table" then
		return nil, "Error decode json: " .. path
	end
	return decoded
end

local function write_file(path, data, mode)
	local file = io.open(path, mode or "wb")
	if not file then
		return nil, "Error write file: " .. path
	end
	file:write(data)
	file:close()
	return true
end

local function sanitize_file_stem(value)
	local stem = tostring(value or "")
	stem = string.gsub(stem, "^%s+", "")
	stem = string.gsub(stem, "%s+$", "")
	stem = string.lower(stem)
	stem = string.gsub(stem, "[^%w_%-]+", "_")
	stem = string.gsub(stem, "_+", "_")
	stem = string.gsub(stem, "^_+", "")
	stem = string.gsub(stem, "_+$", "")
	if stem == "" then
		return "map"
	end
	return stem
end

local function luminance_to_rgba(data, width, height)
	local chunks = {}
	local chunk = {}
	local chunk_size = 0

	for row = height - 1, 0, -1 do
		local row_offset = row * width
		for col = 1, width do
			local value = data:byte(row_offset + col)
			chunk_size = chunk_size + 1
			chunk[chunk_size] = string.char(value, value, value, 255)
			if chunk_size == 1024 then
				chunks[#chunks + 1] = table.concat(chunk)
				chunk = {}
				chunk_size = 0
			end
		end
	end

	if chunk_size > 0 then
		chunks[#chunks + 1] = table.concat(chunk)
	end
	return table.concat(chunks)
end

local function build_atlas_text(map_name, count)
	local lines = {}
	for i = 0, count - 1 do
		lines[#lines + 1] = "images {\n"
			.. "  image: \"/images/clan_maps/" .. map_name .. "/province_" .. i .. ".png\"\n"
			.. "  sprite_trim_mode: SPRITE_TRIM_MODE_OFF\n"
			.. "}"
	end
	lines[#lines + 1] = "extrude_borders: 2"
	return table.concat(lines, "\n")
end

function M.convert_current_map_to_atlas(progress_callback)
	local ok, prepared_or_err = exported_map_format.prepare_map_directory()
	if not ok then
		return nil, prepared_or_err
	end

	local map_data = prepared_or_err or read_json_file((IMAGE_DATA_PATH or "") .. "exported_map/map_info.json")
	if not map_data then
		return nil, "Map data is not available"
	end

	local map_name = sanitize_file_stem(map_data.map_name or map_data.display_name or map_data.name or "map")
	local image_dir = normalize_path((IMAGE_DATA_PATH or "") .. "images/clan_maps/" .. map_name)
	local atlas_dir = normalize_path((IMAGE_DATA_PATH or "") .. "atlas/clan_maps")
	local manifest_dir = normalize_path((IMAGE_DATA_PATH or "") .. "clan_maps")

	local removed, remove_err = remove_path(image_dir)
	if not removed then
		return nil, remove_err
	end
	local made, make_err = ensure_directory(image_dir)
	if not made then
		return nil, make_err
	end
	made, make_err = ensure_directory(atlas_dir)
	if not made then
		return nil, make_err
	end
	made, make_err = ensure_directory(manifest_dir)
	if not made then
		return nil, make_err
	end

	province_binary.for_each(map_data, function(i, province, _, blurred_data)
		if progress_callback then
			progress_callback(i, map_data.num_of_provinces)
		end

		local width = province.size[1]
		local height = province.size[2]
		local rgba = luminance_to_rgba(blurred_data, width, height)
		local png_data = png.encode_rgba(rgba, width, height)
		if not png_data then
			error("Error encode province png: " .. tostring(i))
		end

		local image_path = image_dir .. "/province_" .. (i - 1) .. ".png"
		local written, write_err = write_file(image_path, png_data, "wb")
		if not written then
			error(write_err)
		end
	end)

	local atlas_path = atlas_dir .. "/" .. map_name .. ".atlas"
	local atlas_text = build_atlas_text(map_name, map_data.num_of_provinces)
	local written, write_err = write_file(atlas_path, atlas_text, "w")
	if not written then
		return nil, write_err
	end

	local manifest = {
		map = map_name,
		atlas = "/atlas/clan_maps/" .. map_name .. ".atlas",
		image_dir = "/images/clan_maps/" .. map_name,
		num_of_provinces = map_data.num_of_provinces,
		row_order = "png_top_to_bottom_from_map_bottom_to_top"
	}
	written, write_err = write_file(manifest_dir .. "/" .. map_name .. "_atlas.json", json.encode(manifest), "w")
	if not written then
		return nil, write_err
	end

	return {
		map = map_name,
		atlas = atlas_path,
		images = image_dir,
		manifest = manifest_dir .. "/" .. map_name .. "_atlas.json",
		num_of_provinces = map_data.num_of_provinces
	}
end

return M
