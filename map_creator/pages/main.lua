local M = {}

local gooey = require "gooey.gooey"

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

function M.on_message(self, message_id, message, sender)
end

function M.on_input(self, action_id, action)
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
		msg.post("image:/go#image", "export_map")
	end, update_button_menu)
end

return M