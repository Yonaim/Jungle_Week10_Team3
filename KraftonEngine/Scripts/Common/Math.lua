local Math = {}

function Math.Clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end

    if value > max_value then
        return max_value
    end

    return value
end

function Math.Clamp01(value)
    return Math.Clamp(value, 0.0, 1.0)
end

function Math.SafeNumber(value, fallback)
    if type(value) == "number" then
        return value
    end

    return fallback
end

function Math.SafeNonNegative(value, fallback)
    local number = Math.SafeNumber(value, fallback or 0)
    if number < 0 then
        return 0
    end

    return number
end

function Math.Lerp(a, b, t)
    return a + (b - a) * t
end

function Math.EaseOutQuad(t)
    return 1.0 - (1.0 - t) * (1.0 - t)
end

function Math.EaseInOutQuad(t)
    if t < 0.5 then
        return 2.0 * t * t
    end

    return 1.0 - ((-2.0 * t + 2.0) * (-2.0 * t + 2.0)) * 0.5
end

function Math.Sign(value)
    if value < 0.0 then
        return -1.0
    end

    if value > 0.0 then
        return 1.0
    end

    return 0.0
end

function Math.Round(value)
    if value >= 0.0 then
        return math.floor(value + 0.5)
    end

    return math.ceil(value - 0.5)
end

return Math
