local M = {}
local json = require "scripts.json"
local lume = require "scripts.lume"

local RAW_PROVINCE_DATA_FILE = "province_data.bin"
local COMPRESSED_PROVINCE_DATA_FILE = "province_data.bin.deflate"
local LUMINANCE_STREAM = hash("luminance")
local unpack_values = table.unpack or unpack

local function get_image_data_path()
	return IMAGE_DATA_PATH or ""
end

local function get_exported_map_path()
	return get_image_data_path() .. "exported_map/"
end

local function get_map_info_path()
	return get_exported_map_path() .. "map_info.json"
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

function M.read_map_info()
	return read_json_file(get_map_info_path())
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
	local map_data, err = M.read_map_info()
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

	local ok, write_err = write_json_file(get_map_info_path(), map_data)
	if not ok then
		return nil, write_err
	end

	local cleaned, cleanup_err = M.cleanup_legacy_artifacts()
	if not cleaned then
		return nil, cleanup_err
	end

	return true, map_data
end

return M
