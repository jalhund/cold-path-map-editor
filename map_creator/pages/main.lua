local M = {}

local gooey = require "gooey.gooey"
local province_binary = require "scripts.province_binary"

local function update_button_menu(button)
	if button.pressed_now then
		gui.set_color(button.node, vmath.vector4(0.9, 0.8, 0.6, 1))
	elseif button.released_now then
		gui.set_color(button.node, vmath.vector4(0.4, 0.4, 0.4, 1))
	end
end

local set_page

function M.init()
end

function M.set_callback(callback)
	set_page = callback
end

local errors = {
	[-1] = "ERROR! No texture found for the province! Try to reduce the number of provinces or do not create textures larger than 2044x2044",
	[-2] = "ERROR! Failed to write province_data.bin",
	[-3] = "ERROR! Failed to recreate exported_map",
	[-4] = "ERROR! Failed to compress province_data.bin"
}

function M.on_message(self, message_id, message, sender)
	if message_id == hash("finish_export") then
		self.exporting = false
	elseif message_id == hash("error_code") then
		gui.set_text(gui.get_node("progress_test"), "Error: "..errors[message.code])
	end
end

function update_progress(_, num)
	-- print("Update progress gui:", num)
	gui.set_text(gui.get_node("progress_test"), "Map export progress: "..lume.round(num, .01).."%")
end

function M.on_input(self, action_id, action)
	if self.exporting then
		return
	end
	gooey.button("image_editor_button/outline", action_id, action, function()
		set_page("image_editor")
	end, update_button_menu)
	gooey.button("provinces_editor_button/outline", action_id, action, function()
		set_page("provinces_editor")
	end, update_button_menu)
	gooey.button("autogenerate_provinces/outline", action_id, action, function()
		msg.post("image:/go#image", "generate_provinces")
	end, update_button_menu)
	gooey.button("export_map/outline", action_id, action, function()
		self.exporting = true
		drawpixels.register_progress_callback(update_progress)
		msg.post("image:/go#image", "export_map")
	end, update_button_menu)
	gooey.button("load_map/outline", action_id, action, function()
	    local file = io.open(IMAGE_DATA_PATH.."exported_map/map_info.json", "r")
	    if file then
		    drawpixels.clear_map()
		    
		    local json_data = file:read("*a")
	        file:close()

	        map_data = json.decode(json_data)
	        
	        msg.post("image:/go#image", "set_size", {
	            size = vmath.vector3(map_data.size[1], map_data.size[2], 0)
	        })

            province_binary.for_each(map_data, function(i, province, generated_data)
                print("Load province: ", i, province.position[1], province.position[2], province.size[1], province.size[2], province.water)
                local ok = drawpixels.load_province_data(generated_data, province.position[1], province.position[2], province.size[1], province.size[2], province.water)
                if not ok then
                    error("Error loading province into map creator: " .. i)
                end
            end)

		    msg.post("image:/go#image", "late_init")
		end
	end, update_button_menu)
end

return M
