local M = {}

local exported_map_format = require "scripts.exported_map_format"

local function trim(value)
	value = tostring(value or "")
	value = string.gsub(value, "^%s+", "")
	value = string.gsub(value, "%s+$", "")
	return value
end

local function is_blank(value)
	return not value or trim(value) == ""
end

local function strip_trailing_slash(path)
	path = string.gsub(trim(path), "\\", "/")
	if path == "" or path == "/" then
		return path
	end
	return string.gsub(path, "/+$", "")
end

local function normalize_path(path)
	path = strip_trailing_slash(path)
	if path == "" then
		return ""
	end
	return path .. "/"
end

local function read_config(key)
	if not sys then
		return nil
	end

	if sys.get_config then
		local ok, value = pcall(sys.get_config, key)
		if ok and not is_blank(value) then
			return value
		end
	end

	if sys.get_config_string then
		local ok, value = pcall(sys.get_config_string, key, "")
		if ok and not is_blank(value) then
			return value
		end
	end

	return nil
end

local function get_path_mode(path)
	if not lfs or not lfs.attributes then
		return nil
	end

	local attributes = lfs.attributes(path)
	if attributes then
		return attributes.mode
	end

	attributes = lfs.attributes(string.gsub(path, "/", "\\"))
	return attributes and attributes.mode or nil
end

-- Folder that contains the running executable, used as the default data root
-- so the editor reads/writes maps next to itself (like the game does).
local function get_application_root_path()
	if not sys or not sys.get_application_path then
		return ""
	end

	local ok, path = pcall(sys.get_application_path)
	if not ok or type(path) ~= "string" or path == "" then
		return ""
	end

	local normalized = strip_trailing_slash(path)
	local mode = get_path_mode(normalized)
	if mode == "file" or (not mode and normalized:lower():match("%.exe$")) then
		normalized = normalized:match("^(.*)/[^/]+$") or ""
	end

	return normalize_path(normalized)
end

-- The CLI has a single job: convert an old map to the current .map format.
function M.is_enabled()
	return not is_blank(read_config("cli.convert"))
end

function M.resolve_image_data_path(default_path)
	local path = read_config("cli.data_path")
	if not is_blank(path) then
		return normalize_path(path)
	end

	-- In CLI mode default to the executable's own folder.
	if M.is_enabled() and is_blank(default_path) then
		return get_application_root_path()
	end

	return normalize_path(default_path)
end

function M.run()
	print("CLI convert, data path:", IMAGE_DATA_PATH or "")

	local options = {}
	local output_path = read_config("cli.output_path")
	if not is_blank(output_path) then
		options.output_path = trim(output_path)
	end

	local path, result = exported_map_format.write_map_package(options)
	if not path then
		return nil, result
	end

	print("CLI package saved:", path)
	return true
end

return M
