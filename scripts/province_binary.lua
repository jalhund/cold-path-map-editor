local M = {}

local DEFAULT_FILE_NAME = "province_data.bin"

local function get_file_path(map_data)
	return IMAGE_DATA_PATH .. "exported_map/" .. (map_data.province_data_file or DEFAULT_FILE_NAME)
end

local function read_province_blob(map_data)
	local file_path = get_file_path(map_data)
	local file = io.open(file_path, "rb")
	if not file then
		error("Error open file: " .. file_path)
	end

	local data = file:read("*a")
	file:close()

	if map_data.province_data_compression == "zlib" then
		local zlib_inflate = zlib and zlib.inflate
		if not zlib_inflate then
			error("Error loading province data: zlib.inflate unavailable")
		end

		local ok, inflated_data = pcall(zlib_inflate, data)
		if not ok or not inflated_data then
			error("Error inflate province data: " .. tostring(inflated_data))
		end
		data = inflated_data
	elseif map_data.province_data_compression and map_data.province_data_compression ~= "" then
		error("Unsupported province data compression: " .. tostring(map_data.province_data_compression))
	end

	if map_data.province_data_uncompressed_size and #data ~= map_data.province_data_uncompressed_size then
		error("Error province data size mismatch")
	end

	return data
end

local function read_chunk(data, offset, size, kind, index)
	local next_offset = offset + size
	local chunk = data:sub(offset + 1, next_offset)
	if not chunk or #chunk ~= size then
		error("Error read " .. kind .. " for province " .. index)
	end
	return chunk, next_offset
end

local function ensure_consumed(blob, offset)
	if offset ~= #blob then
		error("Error province_data blob size mismatch")
	end
end

function M.load_all(map_data)
	local blob = read_province_blob(map_data)
	local offset = 0

	for i = 1, map_data.num_of_provinces do
		local province = map_data.provinces[i]
		province.generated_data, offset = read_chunk(blob, offset, province.generated_size, "generated_data", i)
		province.blurred_data, offset = read_chunk(blob, offset, province.blurred_size, "blurred_data", i)
	end

	ensure_consumed(blob, offset)
end

function M.for_each(map_data, callback)
	local blob = read_province_blob(map_data)
	local offset = 0

	for i = 1, map_data.num_of_provinces do
		local province = map_data.provinces[i]
		local generated_data = nil
		local blurred_data = nil

		generated_data, offset = read_chunk(blob, offset, province.generated_size, "generated_data", i)
		blurred_data, offset = read_chunk(blob, offset, province.blurred_size, "blurred_data", i)
		callback(i, province, generated_data, blurred_data)
	end

	ensure_consumed(blob, offset)
end

return M
