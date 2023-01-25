function f1(sx, sy, x, y)
    return (x + y) % 2
end
function f2(sx, sy, x, y)
	fy = y / (sy - 1)
	fx = x / (sx - 1)
	m = 1
	fy = (fy - 0.5) * 2 * m
	fx = (fx - 0.5) * (sx / sy) * m
	return fx * fx + fy * fy
end
function painter(sx, sy, x, y)
	-- return (a + b) % 2 == 1
	-- return true
	-- return ternary((x + y) % 2 == , 1, 0)
	return f1(sx, sy, x, y)
	-- return f2(sx, sy, x, y)
end