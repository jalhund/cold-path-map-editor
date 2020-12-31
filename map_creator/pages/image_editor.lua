local M = {}

local gooey = require "gooey.gooey"

local current_mode = "nothing" -- nothing, draw, line
local current_color = vmath.vector4(1)
drawing_thickness = 1

local set_page

local function update_button_menu(button)
	if button.pressed_now then
		gui.set_color(button.node, vmath.vector4(0.9, 0.8, 0.6, 1))
	elseif button.released_now then
		gui.set_color(button.node, vmath.vector4(0.4, 0.4, 0.4, 1))
	end
	gui.set_color(gui.get_node("tool_"..current_mode.."/outline"), selected_color)
end

local function update_tools()
	gui.set_color(gui.get_node("tool_nothing/outline"), vmath.vector4(0.4, 0.4, 0.4, 1))
	gui.set_color(gui.get_node("tool_draw/outline"), vmath.vector4(0.4, 0.4, 0.4, 1))
	gui.set_color(gui.get_node("tool_line/outline"), vmath.vector4(0.4, 0.4, 0.4, 1))
	gui.set_color(gui.get_node("tool_"..current_mode.."/outline"), selected_color)
end

function M.init()
	current_mode = "nothing"
	update_tools()
	-- Standard 'update_radiobutton(gooey.radio("radio2").set_selected(true))' does not work
	gui.set_color(gui.get_node("thickness_1/outline"), selected_color)

	gui.set_color(gui.get_node("color_white/outline"), selected_color)
	msg.post("image:/go#image", "changed_tool", {
		mode = current_mode
	})

	current_color = vmath.vector4(1)
	msg.post("image:/go#image", "changed_color", {
		color = current_color
	})
end

function M.set_callback(callback)
	set_page = callback
end

function M.on_message(self, message_id, message, sender)
end

local function on_clicked_tool_nothing()
	current_mode = "nothing"
	update_tools()
	msg.post("image:/go#image", "changed_tool", {
		mode = current_mode
	})
	click_for_select = false
end

local function on_clicked_tool_draw()
	current_mode = "draw"
	update_tools()
	msg.post("image:/go#image", "changed_tool", {
		mode = current_mode
	})
	click_for_select = false
end
local function on_clicked_tool_line()
	current_mode = "line"
	update_tools()
	msg.post("image:/go#image", "changed_tool", {
		mode = current_mode
	})
	click_for_select = false
end

function M.on_input(self, action_id, action)
	gooey.button("save/outline", action_id, action, function()
		msg.post("image:/go#image", "save_to_file")
		click_for_select = false
	end, update_button_menu)
	gooey.button("load/outline", action_id, action, function()
		msg.post("image:/go#image", "load_from_file")
		click_for_select = false
	end, update_button_menu)
	gooey.button("image_editor_back/outline", action_id, action, function()
		set_page("main")
		click_for_select = false
	end, update_button_menu)

	gooey.radiogroup("TOOL_GROUP", action_id, action, function(group_id, action_id, action)
			gooey.radio("tool_nothing/outline", group_id, action_id, action, on_clicked_tool_nothing, update_tools)
			gooey.radio("tool_draw/outline", group_id, action_id, action, on_clicked_tool_draw, update_tools)
			gooey.radio("tool_line/outline", group_id, action_id, action, on_clicked_tool_line, update_tools)
		end)

	gooey.radiogroup("COLOR_GROUP", action_id, action, function(group_id, action_id, action)
			gooey.radio("color_white/outline", group_id, action_id, action, function(radio)
				current_color = vmath.vector4(1)
				msg.post("image:/go#image", "changed_color", {
					color = current_color
				})
			end, update_radiobutton)
			gooey.radio("color_black/outline", group_id, action_id, action, function(radio)
				current_color = vmath.vector4(0, 0, 0, 1)
				msg.post("image:/go#image", "changed_color", {
					color = current_color
				})
				-- Because standard selecting in init does not work
				gui.set_color(gui.get_node("color_white/outline"), vmath.vector4(1))
			end, update_radiobutton)
			gooey.radio("color_water/outline", group_id, action_id, action, function(radio)
				current_color = water_color
				msg.post("image:/go#image", "changed_color", {
					color = current_color
				})
				-- Because standard selecting in init does not work
				gui.set_color(gui.get_node("color_white/outline"), vmath.vector4(1))
			end, update_radiobutton)
		end)

	gooey.radiogroup("MYGROUP", action_id, action, function(group_id, action_id, action)
			gooey.radio("thickness_1/outline", group_id, action_id, action, function(radio)
				drawing_thickness = 1
			end, update_radiobutton)
			gooey.radio("thickness_2/outline", group_id, action_id, action, function(radio)
				drawing_thickness = 2
				-- Because standard selecting in init does not work
				gui.set_color(gui.get_node("thickness_1/outline"), vmath.vector4(1))
			end, update_radiobutton)
			gooey.radio("thickness_3/outline", group_id, action_id, action, function(radio)
				drawing_thickness = 3
				-- Because standard selecting in init does not work
				gui.set_color(gui.get_node("thickness_1/outline"), vmath.vector4(1))
			end, update_radiobutton)
		end)

	if not (current_mode == "draw" and action_id == hash("touch")) then
		msg.post("image:/go#image", "scroll", { action_id = action_id, action = action} )
	end
	if action_id == hash("touch") and click_for_select then
		msg.post("image:/go#image", "cursor", { action = action} )
	end
	if action_id == hash("touch") and action.released then
		click_for_select = true
	end
end

return M