local M = {}

local json = require "scripts.json"
local lume = require "scripts.lume"
local map_package = require "scripts.map_package"

local RAW_PROVINCE_DATA_FILE = "province_data.bin"
local COMPRESSED_PROVINCE_DATA_FILE = "province_data.bin.deflate"
local LUMINANCE_STREAM = hash("luminance")
local unpack_values = table.unpack or unpack
local pack_current_exported_map

local function get_image_data_path()
	return IMAGE_DATA_PATH or ""
end

local function get_exported_map_path()
	return get_image_data_path() .. "exported_map/"
end

local function get_map_info_path()
	return get_exported_map_path() .. "map_info.json"
end

local function get_scenario_path()
	return get_exported_map_path() .. "scenario.json"
end

local function read_text_file(path)
	local file = io.open(path, "r")
	if not file then
		return nil, "Error open file: " .. path
	end

	local data = file:read("*a")
	file:close()
	return data
end

local function read_json_file(path)
	local data, err = read_text_file(path)
	if not data then
		return nil, err
	end

	local ok, decoded = pcall(json.decode, data)
	if not ok or type(decoded) ~= "table" then
		return nil, "Error decode json: " .. path
	end

	return decoded
end

local function write_json_file(path, data)
	local file = io.open(path, "w")
	if not file then
		return nil, "Error write file: " .. path
	end

	file:write(json.encode(data))
	file:close()
	return true
end

local function file_exists(path, mode)
	local file = io.open(path, mode or "rb")
	if not file then
		return false
	end

	file:close()
	return true
end

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

local function copy_file(source_path, target_path)
	local source = safe_open_file(source_path, "rb")
	if not source then
		return nil, "Error open file: " .. tostring(source_path)
	end

	local data = source:read("*a")
	source:close()

	local target = safe_open_file(target_path, "wb")
	if not target then
		return nil, "Error write file: " .. tostring(target_path)
	end

	target:write(data)
	target:close()
	return true
end

local function copy_preview_next_to_package(package_path, packed)
	local map_info = packed and packed.map_info or nil
	local preview_file = map_info and map_info.preview_file or nil
	if not package_path or package_path == "" or not preview_file or preview_file == "" then
		return true
	end

	local source_path = get_exported_map_path() .. preview_file
	if not file_exists(source_path, "rb") then
		return true
	end

	local normalized_package_path = tostring(package_path):gsub("\\", "/")
	local package_dir = normalized_package_path:match("^(.*)/[^/]+$") or ""
	local target_path = (package_dir ~= "" and (package_dir .. "/") or "") .. "preview.png"

	return copy_file(source_path, target_path)
end

local function remove_path(path)
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

local function get_legacy_description_path(index)
	return get_exported_map_path() .. "description/" .. index
end

local function get_legacy_generated_path(index)
	return get_exported_map_path() .. "generated_data/" .. index
end

local function get_legacy_blurred_path(index)
	return get_exported_map_path() .. "blurred_data/" .. index
end

local function get_legacy_cleanup_paths()
	local exported_map_path = get_exported_map_path()
	return {
		exported_map_path .. "description",
		exported_map_path .. "generated_data",
		exported_map_path .. "blurred_data"
	}
end

local function validate_legacy_province(province, index)
	if type(province.size) ~= "table" or not province.size[1] or not province.size[2] then
		return nil, "Invalid province size: " .. index
	end

	if type(province.position) ~= "table" or province.position[1] == nil or province.position[2] == nil then
		return nil, "Invalid province position: " .. index
	end

	return true
end

local function stream_to_string(stream, size)
	local chunk = {}
	local chunk_size = 0
	local chunks = {}

	for i = 1, size do
		chunk_size = chunk_size + 1
		chunk[chunk_size] = stream[i]

		if chunk_size == 1024 then
			chunks[#chunks + 1] = string.char(unpack_values(chunk, 1, chunk_size))
			chunk = {}
			chunk_size = 0
		end
	end

	if chunk_size > 0 then
		chunks[#chunks + 1] = string.char(unpack_values(chunk, 1, chunk_size))
	end

	return table.concat(chunks)
end

local function read_legacy_lzs_data(path, width, height)
	local size = width * height
	local buf = buffer.create(size, {
		{
			name = LUMINANCE_STREAM,
			type = buffer.VALUE_TYPE_UINT8,
			count = 1
		}
	})
	local ok = drawpixels.decompress_lzs_data(buf, path, width, height)
	if not ok then
		return nil, "Error decompress file: " .. path
	end

	local stream = buffer.get_stream(buf, LUMINANCE_STREAM)
	return stream_to_string(stream, size)
end

local function compress_province_data(raw_path, compressed_path)
	local file = io.open(raw_path, "rb")
	if not file then
		return nil, "Error open file: " .. raw_path
	end

	local raw_data = file:read("*a")
	file:close()

	local zlib_deflate = zlib and zlib.deflate
	if not zlib_deflate then
		return nil, "zlib.deflate unavailable"
	end

	local ok, compressed_data = pcall(zlib_deflate, raw_data)
	if not ok or not compressed_data then
		return nil, compressed_data or "zlib.deflate failed"
	end

	file = io.open(compressed_path, "wb")
	if not file then
		return nil, "Error create file: " .. compressed_path
	end

	file:write(compressed_data)
	file:close()

	local removed, remove_err = os.remove(raw_path)
	if not removed then
		os.remove(compressed_path)
		return nil, remove_err or "Error remove raw province_data.bin"
	end

	return {
		compressed_size = #compressed_data,
		uncompressed_size = #raw_data
	}
end

local function read_current_map_info()
	return read_json_file(get_map_info_path())
end

local function read_current_scenario()
	return read_json_file(get_scenario_path())
end

local function get_file_stem(path)
	local normalized = tostring(path or ""):gsub("\\", "/")
	local file_name = normalized:match("([^/]+)$") or normalized
	return file_name:match("^(.*)%.map$") or file_name
end

local function sort_map_package_files(files)
	table.sort(files, function(a, b)
		local a_name = a:lower()
		local b_name = b:lower()
		if a_name == "exported_map.map" then
			return true
		end
		if b_name == "exported_map.map" then
			return false
		end
		return a_name < b_name
	end)
end

function M.find_map_package_path()
	local root = get_image_data_path()
	if not root or root == "" then
		root = ""
	end
	local scan_root = root ~= "" and root or "."

	local files = {}
	for entry in lfs.dir(scan_root) do
		if entry ~= "." and entry ~= ".." and entry:match("%.map$") then
			local path = root .. entry
			if file_exists(path, "rb") then
				files[#files + 1] = entry
			end
		end
	end

	if #files == 0 then
		return nil
	end

	sort_map_package_files(files)
	return root .. files[1], #files
end

function M.read_map_info()
	local map_data = read_current_map_info()
	if map_data then
		return map_data
	end

	local package_path = M.find_map_package_path()
	if not package_path then
		return nil
	end

	local package_map_data = map_package.read_map_info(package_path)
	return package_map_data
end

function M.is_new_format(map_data)
	return type(map_data) == "table" and type(map_data.provinces) == "table"
end

function M.is_legacy_format(map_data)
	if type(map_data) ~= "table" or M.is_new_format(map_data) then
		return false
	end

	if type(map_data.num_of_provinces) ~= "number" or map_data.num_of_provinces < 1 then
		return false
	end

	return file_exists(get_legacy_description_path(1), "r")
		and file_exists(get_legacy_generated_path(1), "rb")
		and file_exists(get_legacy_blurred_path(1), "rb")
end

function M.has_legacy_artifacts()
	return file_exists(get_legacy_description_path(1), "r")
		or file_exists(get_legacy_generated_path(1), "rb")
		or file_exists(get_legacy_blurred_path(1), "rb")
end

function M.cleanup_legacy_artifacts()
	local cleanup_paths = get_legacy_cleanup_paths()

	for i = 1, #cleanup_paths do
		local removed, remove_err = remove_path(cleanup_paths[i])
		if not removed then
			return nil, remove_err
		end
	end

	return true
end

function M.convert_legacy_to_new()
	local map_data, err = read_current_map_info()
	if not map_data then
		return nil, err
	end

	if M.is_new_format(map_data) then
		return true, map_data
	end

	if not M.is_legacy_format(map_data) then
		return nil, "Legacy exported_map not found"
	end

	local provinces = {}
	local exported_map_path = get_exported_map_path()
	local raw_path = exported_map_path .. RAW_PROVINCE_DATA_FILE
	local raw_file = io.open(raw_path, "wb")
	if not raw_file then
		return nil, "Error create file: " .. raw_path
	end

	for i = 1, map_data.num_of_provinces do
		local province, province_err = read_json_file(get_legacy_description_path(i))
		if not province then
			raw_file:close()
			os.remove(raw_path)
			return nil, province_err
		end

		local valid, validation_err = validate_legacy_province(province, i)
		if not valid then
			raw_file:close()
			os.remove(raw_path)
			return nil, validation_err
		end

		local width = province.size[1]
		local height = province.size[2]
		local generated_data, generated_err = read_legacy_lzs_data(get_legacy_generated_path(i), width, height)
		if not generated_data then
			raw_file:close()
			os.remove(raw_path)
			return nil, generated_err
		end

		local blurred_data, blurred_err = read_legacy_lzs_data(get_legacy_blurred_path(i), width, height)
		if not blurred_data then
			raw_file:close()
			os.remove(raw_path)
			return nil, blurred_err
		end

		province.generated_size = #generated_data
		province.blurred_size = #blurred_data
		provinces[i] = province

		raw_file:write(generated_data)
		raw_file:write(blurred_data)
	end

	raw_file:close()

	local province_data_info, compression_err = compress_province_data(
		raw_path,
		exported_map_path .. COMPRESSED_PROVINCE_DATA_FILE
	)
	if not province_data_info then
		return nil, compression_err
	end

	if not map_data.id and lume and lume.uuid then
		map_data.id = lume.uuid()
	end

	map_data.province_data_file = COMPRESSED_PROVINCE_DATA_FILE
	map_data.province_data_compression = "zlib"
	map_data.province_data_uncompressed_size = province_data_info.uncompressed_size
	map_data.province_data_compressed_size = province_data_info.compressed_size
	map_data.provinces = provinces

	local scenario = read_current_scenario()
	if scenario then
		scenario.map = map_data.id
		if map_data.map_name and map_data.map_name ~= "" then
			scenario.map_name = map_data.map_name
		end
		local scenario_ok, scenario_err = write_json_file(get_scenario_path(), scenario)
		if not scenario_ok then
			return nil, scenario_err
		end
	end

	local ok, write_err = write_json_file(get_map_info_path(), map_data)
	if not ok then
		return nil, write_err
	end

	local cleaned, cleanup_err = M.cleanup_legacy_artifacts()
	if not cleaned then
		return nil, cleanup_err
	end

	local packed = pack_current_exported_map()
	if packed and type(packed) == "table" and packed.path then
		map_data.package_path = packed.path
	end

	return true, map_data
end

function M.detect_format()
	local current_map_data = read_current_map_info()
	if current_map_data then
		if M.is_legacy_format(current_map_data) then
			return {
				kind = "legacy",
				map_data = current_map_data
			}
		end

		if M.is_new_format(current_map_data) then
			return {
				kind = "folder",
				map_data = current_map_data
			}
		end
	end

	local package_path = M.find_map_package_path()
	if package_path then
		local package_map_data, manifest_or_err = map_package.read_map_info(package_path)
		return {
			kind = "package",
			map_data = package_map_data,
			package_path = package_path,
			error = package_map_data and nil or manifest_or_err
		}
	end

	return {
		kind = "missing",
		map_data = current_map_data
	}
end

function M.get_format_kind()
	return M.detect_format().kind
end

local function default_map_name()
	return "map"
end

local function get_package_name_defaults(package_path)
	if not package_path or package_path == "" then
		return nil, nil
	end

	local stem = map_package.sanitize_file_stem(get_file_stem(package_path))
	return stem, map_package.prettify_map_name(stem)
end

function M.resolve_package_target(options)
	local output_path = options and options.output_path or nil
	if not output_path or output_path == "" then
		output_path = M.find_map_package_path()
	end

	local existing_package_map_name, existing_package_display_name = get_package_name_defaults(output_path)
	if output_path and output_path ~= "" then
		return {
			output_path = output_path,
			map_name = existing_package_map_name or default_map_name(),
			display_name = (options and options.display_name) or existing_package_display_name or map_package.prettify_map_name(existing_package_map_name)
		}
	end

	local current_map_data = read_current_map_info() or {}
	local current_scenario = read_current_scenario() or {}
	local desired_name = (options and options.map_name)
		or current_map_data.map_name
		or current_scenario.map_name
		or default_map_name()
	local map_name = map_package.sanitize_file_stem(desired_name)

	return {
		output_path = M.build_map_package_path(map_name),
		map_name = map_name,
		display_name = (options and options.display_name)
			or current_map_data.display_name
			or current_map_data.name
			or current_scenario.name
			or map_package.prettify_map_name(map_name)
	}
end

local function sync_exported_map_identity(map_name, display_name)
	local map_info = read_current_map_info()
	if map_info then
		map_info.map_name = map_name
		if not map_info.display_name or map_info.display_name == "" then
			map_info.display_name = display_name
		end
		if not map_info.name or map_info.name == "" then
			map_info.name = map_info.display_name
		end
		write_json_file(get_map_info_path(), map_info)
	end

	local scenario = read_current_scenario()
	if scenario then
		scenario.map_name = map_name
		write_json_file(get_scenario_path(), scenario)
	end
end

pack_current_exported_map = function(options)
	local map_data = read_current_map_info()
	if not map_data or not M.is_new_format(map_data) then
		return nil, "Current folder map is missing"
	end

	local target = M.resolve_package_target(options)

	return map_package.pack_map_directory(get_exported_map_path(), target.output_path, {
		map_name = target.map_name,
		display_name = target.display_name
	})
end

function M.read_metadata_defaults()
	local detected = M.detect_format()
	local map_data = detected.map_data or {}
	local scenario_data = nil
	local package_path = M.find_map_package_path()
	local package_map_name, package_display_name = get_package_name_defaults(package_path)

	if detected.kind == "folder" or detected.kind == "legacy" then
		scenario_data = read_current_scenario()
	elseif detected.kind == "package" and detected.package_path then
		local unpacked = map_package.read_package(detected.package_path)
		if unpacked then
			local scenario_raw = map_package.read_section(unpacked, "generated_scenario")
			if scenario_raw then
				local ok, decoded = pcall(json.decode, scenario_raw)
				if ok and type(decoded) == "table" then
					scenario_data = decoded
				end
			end
		end
	end

	local map_name = package_map_name
		or map_data.map_name
		or (scenario_data and scenario_data.map_name)
		or default_map_name()
	local display_name = map_data.display_name
		or map_data.name
		or (scenario_data and scenario_data.name)
		or package_display_name
		or map_package.prettify_map_name(map_name)

	return {
		map_name = map_name,
		display_name = display_name
	}
end

function M.prepare_map_directory()
	local detected = M.detect_format()

	if detected.kind == "legacy" then
		return M.convert_legacy_to_new()
	end

	if detected.kind == "package" then
		if not detected.package_path then
			return nil, "Map package not found"
		end

		local unpacked, err = map_package.unpack_to_directory(detected.package_path, get_exported_map_path())
		if not unpacked then
			return nil, err
		end

		local package_map_name, package_display_name = get_package_name_defaults(detected.package_path)
		if package_map_name then
			sync_exported_map_identity(package_map_name, package_display_name or map_package.prettify_map_name(package_map_name))
			unpacked.map_info.map_name = package_map_name
			if not unpacked.map_info.display_name or unpacked.map_info.display_name == "" then
				unpacked.map_info.display_name = package_display_name
			end
		end

		return true, unpacked.map_info
	end

	if detected.kind == "folder" then
		return true, detected.map_data
	end

	return nil, "Map data not found"
end

function M.build_map_package_path(map_name)
	return map_package.build_default_package_path(get_image_data_path(), map_name)
end

function M.write_map_package(options)
	local prepared, map_data_or_err = M.prepare_map_directory()
	if not prepared then
		return nil, map_data_or_err
	end

	local packed, err = pack_current_exported_map(options)
	if not packed then
		return nil, err
	end

	local preview_copied, preview_err = copy_preview_next_to_package(packed.path, packed)
	if not preview_copied then
		return nil, preview_err
	end

	return packed.path, packed
end

return M
