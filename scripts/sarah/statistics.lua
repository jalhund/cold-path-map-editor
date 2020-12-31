local M = {}

local state = {}

function M.add(state_id, value)
	if not state[state_id] then
		state[state_id] = {}
	end
	state[state_id][#state[state_id] + 1] = value
end

function M.results(state_id)
	local min = 0
	local max = 0
	local total = 0
	local average = 0

	for k, v in pairs(state[state_id]) do
		if v < min then
			min = v
		end
		if v > max then
			max = v
		end
		total = total + v
	end
	average = total / #state[state_id]

	print("State results: ", state_id)
	print("Min and max: ", min, max)
	print("Total and average: ", total, average)
end

function M.remove_state(state_id)
	state[state_id] = nil
end

return M