selected_color = vmath.vector4(1,1,0.4,1)
water_color = vmath.vector4(0.71,0.82,0.93,1)

function update_radiobutton(radiobutton, state)
	if radiobutton.pressed_now then
		click_for_select = false
	end
	if radiobutton.selected_now then
		gui.set_color(radiobutton.node, selected_color)
	elseif radiobutton.deselected_now then
		gui.set_color(radiobutton.node, vmath.vector4(1))
	end
end

function color_to_vector(t)
	return vmath.vector4(t[1]/255, t[2]/255, t[3]/255, 1)
end