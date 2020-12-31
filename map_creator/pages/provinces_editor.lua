local M = {}

local gooey = require "gooey.gooey"
local generate_color = require "scripts.generate_color"
local current_color = vmath.vector4(1)

local function update_button_menu(button)
	if button.pressed_now then
		gui.set_color(button.node, vmath.vector4(0.9, 0.8, 0.6, 1))
	elseif button.released_now then
		gui.set_color(button.node, vmath.vector4(0.4, 0.4, 0.4, 1))
	end
end

local function set_current_color(c)
	current_color = c
	local x, y, z = c.x*255, c.y*255, c.z*255
	x, y, z = math.floor(x), math.floor(y), math.floor(z)
	gui.set_text(gui.get_node("provinces_editor_color_text"), "R: "..x.."\nG: "..y.."\nB: "..z)
	gui.set_color(gui.get_node("provinces_editor_color"), c)
end

local set_page

function M.init()
	gui.set_color(gui.get_node("provinces_editor_pipette/outline"), selected_color)
	msg.post("image:/go#image", "changed_tool", {
		mode = "pipette"
	})
	set_current_color(vmath.vector4(1))
end

function M.set_callback(callback)
	set_page = callback
end

function M.on_message(self, message_id, message, sender)
	if message_id == hash("changed_color") then
		set_current_color(message.color)
	end
end

local function on_clicked_tool(tool)
	msg.post("image:/go#image", "changed_tool", {
		mode = tool
	})
	click_for_select = false
end

local function on_clicked_generate()
	local generated_color = generate_color.ground()

	set_current_color(generated_color)
	msg.post("image:/go#image", "changed_color", {
		color = generated_color
	})
	click_for_select = false
end

local function on_clicked_generate_water()
	local generated_color = generate_color.water()

	set_current_color(generated_color)
	msg.post("image:/go#image", "changed_color", {
		color = generated_color
	})
	click_for_select = false
end

function M.on_input(self, action_id, action)

	gooey.radiogroup("PROVINCES_TOOL_GROUP", action_id, action, function(group_id, action_id, action)
		gooey.radio("provinces_editor_pipette/outline", group_id, action_id, action, function()
			on_clicked_tool("pipette")
		end, update_radiobutton)
		gooey.radio("provinces_editor_fill/outline", group_id, action_id, action, function()
			on_clicked_tool("fill")
			-- Because standard selecting in init does not work
			gui.set_color(gui.get_node("provinces_editor_pipette/outline"), vmath.vector4(1))
		end, update_radiobutton)
	end)

	gooey.button("provinces_editor_generate/outline", action_id, action, on_clicked_generate, update_button_menu)
	gooey.button("provinces_editor_generate_water/outline", action_id, action, on_clicked_generate_water, update_button_menu)
	gooey.button("provinces_editor_back/outline", action_id, action, function()
		set_page("main")
		click_for_select = false
	end, update_button_menu)

	msg.post("image:/go#image", "scroll", { action_id = action_id, action = action} )
	if action_id == hash("touch") and click_for_select then
		msg.post("image:/go#image", "cursor", { action = action} )
	end
	if action_id == hash("touch") and action.released then
		click_for_select = true
	end
end

return M