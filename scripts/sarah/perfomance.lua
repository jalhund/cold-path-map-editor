local M = {}

local begin_time = {}
local state = {}

function M.start(id)
	begin_time[id or "id_"..math.floor(math.random(0, 10))] = socket.gettime()
	return id
end

function M.finish(id)
	local t = socket.gettime() - begin_time[id]
	print("seconds elapsed: ", id, t)
	begin_time[id] = nil
end

function M.register_state(state_id)
	state[state_id] = {
		0, {}, {}
	}
end

function M.continue_state(state_id)
	state[state_id][1] = state[state_id][1] + 1
	local i = state[state_id][1]
	state[state_id][2][i] = socket.gettime()
end

function M.finish_state(state_id)
	local i = state[state_id][1]
	state[state_id][3][i] = socket.gettime()
end

function M.state_results(state_id)
	local min = 0
	local max = 0
	local total = 0
	local average = 0

	for k, v in pairs(state[state_id][3]) do
		local delta = state[state_id][3][k] - state[state_id][2][k]
		if delta < min then
			min = delta
		end
		if delta > max then
			max = delta
		end
		total = total + delta
	end
	average = total / state[state_id][1]

	print("State results: ", state_id)
	print("Min and max: ", min, max)
	print("Total and average: ", total, average)
end

function M.remove_state(state_id)
	state[state_id] = nil
end

return M