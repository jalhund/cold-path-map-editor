local M = {}

local gooey = require "gooey.gooey"
local exported_map_format = require "scripts.exported_map_format"
local province_binary = require "scripts.province_binary"

local function update_button_menu(button)
	if button.pressed_now then
		gui.set_color(button.node, vmath.vector4(0.9, 0.8, 0.6, 1))
	elseif button.released_now then
		gui.set_color(button.node, vmath.vector4(0.4, 0.4, 0.4, 1))
	end
end

local set_page
local LOAD_MAP_TEXT = "Load map"
local CONVERT_MAP_TEXT = "Convert legacy map"
local LOAD_PACKAGE_TEXT = "Load .map"

local function set_load_map_text(text)
	gui.set_text(gui.get_node("load_map_text"), text)
end

local function refresh_load_map_button(self)
	local kind = exported_map_format.get_format_kind()
	self.has_legacy_exported_map = kind == "legacy"
	self.has_map_package = kind == "package"
	if self.has_legacy_exported_map then
		set_load_map_text(CONVERT_MAP_TEXT)
	elseif self.has_map_package then
		set_load_map_text(LOAD_PACKAGE_TEXT)
	else
		set_load_map_text(LOAD_MAP_TEXT)
	end
end

local function load_map_into_editor(map_data)
	drawpixels.clear_map()

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

function M.init()
	refresh_load_map_button({})
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
		refresh_load_map_button(self)
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

	refresh_load_map_button(self)

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
		local kind = exported_map_format.get_format_kind()
		if kind == "legacy" then
			gui.set_text(gui.get_node("progress_test"), "Converting legacy exported_map...")
		elseif kind == "package" then
			gui.set_text(gui.get_node("progress_test"), "Unpacking map package...")
		end

		local ok, map_data_or_err = exported_map_format.prepare_map_directory()
		if ok then
			if kind == "legacy" then
				gui.set_text(gui.get_node("progress_test"), "Legacy map converted")
			elseif kind == "package" then
				gui.set_text(gui.get_node("progress_test"), "Map package loaded")
			end
			refresh_load_map_button(self)
			load_map_into_editor(map_data_or_err)
		else
			gui.set_text(gui.get_node("progress_test"), "Error: " .. tostring(map_data_or_err))
		end
	end, update_button_menu)
end

return M
