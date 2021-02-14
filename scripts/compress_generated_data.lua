-- This module should be used only if you need to translate the map into in-game format (if EXPORT_TO_PPM in drawpixels.cpp is true)

local M = {}

function M.work(n)
	for i = 1, n do
		local file = io.open("exported_map/generated_data/"..i, "r")
		local data = file:read("*a")
		local new_file = io.open("new_generated_data/"..i..".data", "wb")
		print("Data: ", i, #data, #zlib.deflate(data))
		new_file:write(zlib.deflate(data))
		file:close()
		new_file:close()
	end
end

return M
