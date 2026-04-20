local M = {}

local json = require "scripts.json"

local MAGIC = "CPMP"
local VERSION = 1
local PACKAGE_KIND = "cold_path_map"
local DEFAULT_PROVINCE_DATA_FILE = "province_data.bin"

local SECTION_ORDER = {
	"map_info",
	"offsets",
	"adjacency",
	"preview",
	"generated_scenario",
	"province_data"
}

local DEFAULT_SECTION_FILES = {
	map_info = "map_info.json",
	offsets = "offsets.json",
	adjacency = "adjacency.dat",
	preview = "preview.png",
	generated_scenario = "scenario.json"
}

local function safe_open_file(path, mode)
	local file = io.open(path, mode)
	if file then
		return file
	end

	local normalized = string.gsub(path, "/", "\\")
	file = io.open(normalized, mode)
	if file then
		return file
	end

	normalized = string.gsub(path, "\\", "/")
	return io.open(normalized, mode)
end

local function read_file(path, mode)
	local file = safe_open_file(path, mode or "rb")
	if not file then
		return nil, "Error open file: " .. tostring(path)
	end

	local data = file:read("*a")
	file:close()
	return data
end

local function write_file(path, data)
	local file = safe_open_file(path, "wb")
	if not file then
		return nil, "Error write file: " .. tostring(path)
	end

	file:write(data)
	file:close()
	return true
end

local function remove_path(path)
	local attributes = lfs.attributes(path)
	if not attributes then
		return true
	end

	if attributes.mode ~= "directory" then
		local ok, err = os.remove(path)
		if not ok then
			return nil, err or ("Error remove file: " .. tostring(path))
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
		return nil, err or ("Error remove directory: " .. tostring(path))
	end
	return true
end

local function ensure_directory(path)
	if not path or path == "" then
		return true
	end

	local attributes = lfs.attributes(path)
	if attributes and attributes.mode == "directory" then
		return true
	end

	local parent = path:match("^(.*)[/\\][^/\\]+$")
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

	return nil, err or ("Error create directory: " .. tostring(path))
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

local function prettify_map_name(map_name)
	local value = tostring(map_name or "")
	value = string.gsub(value, "_", " ")
	value = string.gsub(value, "%s+", " ")
	value = string.gsub(value, "^%s+", "")
	value = string.gsub(value, "%s+$", "")
	if value == "" then
		return "Map"
	end
	return string.gsub(value, "(%a)([%w']*)", function(first, rest)
		return string.upper(first) .. string.lower(rest)
	end)
end

local function encode_u32(value)
	local number = tonumber(value) or 0
	local b1 = math.floor(number / 16777216) % 256
	local b2 = math.floor(number / 65536) % 256
	local b3 = math.floor(number / 256) % 256
	local b4 = number % 256
	return string.char(b1, b2, b3, b4)
end

local function decode_u32(data, offset)
	local b1, b2, b3, b4 = string.byte(data, offset, offset + 3)
	if not b4 then
		return nil, nil, "Unexpected end of package header"
	end

	return (((b1 * 256) + b2) * 256 + b3) * 256 + b4, offset + 4
end

local function get_section_path(map_dir, map_info, section_id)
	if section_id == "province_data" then
		local file_name = map_info.province_data_file or DEFAULT_PROVINCE_DATA_FILE
		return map_dir .. file_name, file_name
	end

	local file_name = DEFAULT_SECTION_FILES[section_id]
	if not file_name then
		return nil, nil
	end

	if section_id == "preview" and map_info.preview_file and map_info.preview_file ~= "" then
		file_name = map_info.preview_file
	end

	return map_dir .. file_name, file_name
end

local function build_section_descriptor(section_id, file_name, offset, size, map_info, scenario_data)
	local descriptor = {
		id = section_id,
		file_name = file_name,
		offset = offset,
		size = size
	}

	if section_id == "map_info" or section_id == "offsets" or section_id == "generated_scenario" then
		descriptor.encoding = "json"
	elseif section_id == "adjacency" then
		descriptor.encoding = "text"
	elseif section_id == "preview" then
		descriptor.encoding = "png"
	else
		descriptor.encoding = "binary"
	end

	if section_id == "province_data" then
		descriptor.compression = map_info.province_data_compression
		descriptor.uncompressed_size = map_info.province_data_uncompressed_size
		descriptor.compressed_size = map_info.province_data_compressed_size
	end

	if section_id == "generated_scenario" and type(scenario_data) == "table" then
		descriptor.scenario_id = scenario_data.id
	end

	return descriptor
end

local function index_sections(manifest)
	local sections_by_id = {}
	for _, section in ipairs(manifest.sections or {}) do
		sections_by_id[section.id] = section
	end
	manifest.sections_by_id = sections_by_id
	return manifest
end

function M.is_package_bytes(data)
	return type(data) == "string" and data:sub(1, #MAGIC) == MAGIC
end

function M.read_package_bytes(data)
	if not M.is_package_bytes(data) then
		return nil, "Invalid map package magic"
	end

	local offset = #MAGIC + 1
	local version = string.byte(data, offset)
	if not version then
		return nil, "Invalid map package version"
	end
	offset = offset + 1

	local manifest_size, next_offset, err = decode_u32(data, offset)
	if not manifest_size then
		return nil, err
	end
	offset = next_offset

	local manifest_json = data:sub(offset, offset + manifest_size - 1)
	if #manifest_json ~= manifest_size then
		return nil, "Invalid map package manifest size"
	end
	offset = offset + manifest_size

	local ok, manifest = pcall(json.decode, manifest_json)
	if not ok or type(manifest) ~= "table" then
		return nil, "Invalid map package manifest"
	end

	if manifest.kind ~= PACKAGE_KIND then
		return nil, "Unsupported map package kind"
	end
	if manifest.version ~= version then
		return nil, "Map package version mismatch"
	end

	manifest = index_sections(manifest)

	return {
		version = version,
		manifest = manifest,
		payload_offset = offset,
		bytes = data
	}
end

function M.read_package(path)
	local data, err = read_file(path, "rb")
	if not data then
		return nil, err
	end

	local package_data, read_err = M.read_package_bytes(data)
	if not package_data then
		return nil, read_err
	end

	package_data.path = path
	return package_data
end

function M.read_section(package_data, section_id)
	if type(package_data) ~= "table" or type(package_data.manifest) ~= "table" then
		return nil, "Package data is required"
	end

	local section = package_data.manifest.sections_by_id and package_data.manifest.sections_by_id[section_id] or nil
	if not section then
		return nil, "Missing section: " .. tostring(section_id)
	end

	local start_offset = package_data.payload_offset + section.offset
	local finish_offset = start_offset + section.size - 1
	local bytes = package_data.bytes:sub(start_offset, finish_offset)
	if #bytes ~= section.size then
		return nil, "Invalid section size: " .. tostring(section_id)
	end

	return bytes, section
end

function M.read_map_info(path)
	local package_data, err = M.read_package(path)
	if not package_data then
		return nil, err
	end

	local bytes, section_err = M.read_section(package_data, "map_info")
	if not bytes then
		return nil, section_err
	end

	local ok, map_info = pcall(json.decode, bytes)
	if not ok or type(map_info) ~= "table" then
		return nil, "Invalid map_info section"
	end

	return map_info, package_data.manifest
end

function M.pack_map_directory(map_dir, output_path, options)
	local map_info_path = map_dir .. "map_info.json"
	local offsets_path = map_dir .. "offsets.json"
	local adjacency_path = map_dir .. "adjacency.dat"
	local scenario_path = map_dir .. "scenario.json"

	local map_info_raw, err = read_file(map_info_path, "rb")
	if not map_info_raw then
		return nil, err
	end

	local ok, map_info = pcall(json.decode, map_info_raw)
	if not ok or type(map_info) ~= "table" then
		return nil, "Invalid map_info.json"
	end

	local scenario_raw, scenario_err = read_file(scenario_path, "rb")
	if not scenario_raw then
		return nil, scenario_err
	end

	local scenario_ok, scenario_data = pcall(json.decode, scenario_raw)
	if not scenario_ok or type(scenario_data) ~= "table" then
		return nil, "Invalid scenario.json"
	end

	local payload_chunks = {}
	local manifest_sections = {}
	local payload_offset = 0

	for _, section_id in ipairs(SECTION_ORDER) do
		local section_path, file_name = get_section_path(map_dir, map_info, section_id)
		if section_path then
			local bytes = nil
			if section_id == "map_info" then
				bytes = map_info_raw
			elseif section_id == "generated_scenario" then
				bytes = scenario_raw
			else
				bytes = read_file(section_path, "rb")
			end

			if bytes then
				payload_chunks[#payload_chunks + 1] = bytes
				manifest_sections[#manifest_sections + 1] = build_section_descriptor(
					section_id,
					file_name,
					payload_offset,
					#bytes,
					map_info,
					scenario_data
				)
				payload_offset = payload_offset + #bytes
			elseif section_id ~= "preview" then
				return nil, "Missing section file: " .. tostring(section_path)
			end
		end
	end

	local map_name = map_info.map_name
		or scenario_data.map_name
		or (options and options.map_name)
		or "map"
	local display_name = map_info.display_name
		or map_info.name
		or (options and options.display_name)
		or prettify_map_name(map_name)

	local manifest = {
		kind = PACKAGE_KIND,
		version = VERSION,
		map_id = map_info.id,
		map_name = map_name,
		display_name = display_name,
		size = map_info.size,
		num_of_provinces = map_info.num_of_provinces,
		preview_file = map_info.preview_file,
		preview_size = map_info.preview_size,
		province_data_file = map_info.province_data_file,
		province_data_compression = map_info.province_data_compression,
		province_data_uncompressed_size = map_info.province_data_uncompressed_size,
		province_data_compressed_size = map_info.province_data_compressed_size,
		generated_scenario_id = scenario_data.id,
		sections = manifest_sections
	}

	local manifest_json = json.encode(manifest)
	local package_bytes = table.concat({
		MAGIC,
		string.char(VERSION),
		encode_u32(#manifest_json),
		manifest_json,
		table.concat(payload_chunks)
	})

	local ok_write, write_err = write_file(output_path, package_bytes)
	if not ok_write then
		return nil, write_err
	end

	return {
		path = output_path,
		map_info = map_info,
		manifest = index_sections(manifest)
	}
end

function M.unpack_to_directory(package_path, output_dir)
	local package_data, err = M.read_package(package_path)
	if not package_data then
		return nil, err
	end

	local cleared, clear_err = remove_path(output_dir)
	if not cleared then
		return nil, clear_err
	end

	local created, create_err = ensure_directory(output_dir)
	if not created then
		return nil, create_err
	end

	for _, section in ipairs(package_data.manifest.sections or {}) do
		local bytes, section_err = M.read_section(package_data, section.id)
		if not bytes then
			return nil, section_err
		end

		local ok, write_err = write_file(output_dir .. section.file_name, bytes)
		if not ok then
			return nil, write_err
		end
	end

	local map_info_bytes, map_info_err = M.read_section(package_data, "map_info")
	if not map_info_bytes then
		return nil, map_info_err
	end

	local ok, map_info = pcall(json.decode, map_info_bytes)
	if not ok or type(map_info) ~= "table" then
		return nil, "Invalid map_info section"
	end

	return {
		path = package_path,
		map_info = map_info,
		manifest = package_data.manifest
	}
end

function M.build_default_package_path(root_path, map_name)
	return tostring(root_path or "") .. sanitize_file_stem(map_name) .. ".map"
end

function M.sanitize_file_stem(value)
	return sanitize_file_stem(value)
end

function M.prettify_map_name(map_name)
	return prettify_map_name(map_name)
end

return M
