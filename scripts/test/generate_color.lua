local M = {}

local generate_color = require "scripts.generate_color"

local used_colors = {}

-- For test need remove color_to_vector in return in generate_color.lua
function M.start(n)
	local n = n or 10000
	for i = 1, n do
		local t = generate_color.ground()
		if lume.match(used_colors, function(x)
			if x[1] == t[1] and x[2] == t[2] and x[3] == t[3] then
				pprint(x)
			end
			return x[1] == t[1] and x[2] == t[2] and x[3] == t[3]
		end) then
			pprint("Found color: ", i, t)
		end
		table.insert(used_colors, t)
	end
end

return M