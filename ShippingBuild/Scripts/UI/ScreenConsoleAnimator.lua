local ScreenConsoleAnimator = {}

local TINT_CLEAR = { 0.0, 0.0, 0.0, 0.0 }
local TINT_FINAL = { 1.0, 1.0, 1.0, 1.0 }

local POWER_FLASH = { 0.42, 0.96, 1.0, 0.88 }
local POWER_DIM = { 0.26, 0.72, 0.82, 0.22 }
local POWER_RISE = { 0.76, 1.0, 1.0, 0.74 }
local POWER_FLICKER = { 0.36, 0.82, 0.92, 0.30 }

local SWAP_FLASH = { 0.48, 0.92, 1.0, 0.66 }
local SWAP_HOLD = { 0.74, 1.0, 1.0, 0.84 }
local SWAP_FLICKER = { 0.58, 0.88, 0.96, 0.42 }
local SWAP_SETTLE = { 0.90, 1.0, 1.0, 0.92 }

local function clamp01(value)
    if value < 0.0 then
        return 0.0
    end

    if value > 1.0 then
        return 1.0
    end

    return value
end

local function lerp(a, b, t)
    return a + (b - a) * clamp01(t)
end

local function lerp_tint(from_tint, to_tint, t)
    return {
        lerp(from_tint[1], to_tint[1], t),
        lerp(from_tint[2], to_tint[2], t),
        lerp(from_tint[3], to_tint[3], t),
        lerp(from_tint[4], to_tint[4], t),
    }
end

local function set_visible(component, visible)
    if component then
        component:SetVisible(visible)
    end
end

local function set_texture(component, texture_path)
    if component and texture_path and texture_path ~= "" then
        component:SetTexture(texture_path)
    end
end

local function set_tint(component, tint)
    if component then
        component:SetTint(tint[1], tint[2], tint[3], tint[4])
    end
end

local function animate_tint(component, from_tint, to_tint, duration, steps)
    if not component then
        return
    end

    local frame_count = steps or 5
    if duration <= 0.0 or frame_count <= 0 then
        set_tint(component, to_tint)
        return
    end

    local step_duration = duration / frame_count
    local index = 1
    while index <= frame_count do
        local t = index / frame_count
        set_tint(component, lerp_tint(from_tint, to_tint, t))
        wait(step_duration)
        index = index + 1
    end
end

function ScreenConsoleAnimator.new(base_component, overlay_component)
    local self = {
        base = base_component,
        overlay = overlay_component,
        current_texture = nil,
        busy = false,
    }

    function self:is_busy()
        return self.busy
    end

    function self:is_idle()
        return not self.busy
    end

    function self:reset_to_off(off_texture, preload_texture)
        self.current_texture = off_texture

        set_visible(self.base, true)
        set_visible(self.overlay, true)

        set_texture(self.base, off_texture)
        set_tint(self.base, TINT_FINAL)

        if preload_texture then
            set_texture(self.overlay, preload_texture)
        end
        set_tint(self.overlay, TINT_CLEAR)
    end

    function self:play_power_on(target_texture)
        if self.busy then
            return false
        end

        self.busy = true
        set_visible(self.base, true)
        set_visible(self.overlay, true)
        set_texture(self.overlay, target_texture)
        set_tint(self.overlay, TINT_CLEAR)

        wait(0.20)
        animate_tint(self.overlay, TINT_CLEAR, POWER_FLASH, 0.04, 3)
        animate_tint(self.overlay, POWER_FLASH, POWER_DIM, 0.06, 4)
        animate_tint(self.overlay, POWER_DIM, POWER_RISE, 0.05, 4)
        animate_tint(self.overlay, POWER_RISE, POWER_FLICKER, 0.04, 3)
        animate_tint(self.overlay, POWER_FLICKER, TINT_FINAL, 0.11, 6)

        self.current_texture = target_texture
        set_texture(self.base, self.current_texture)
        set_tint(self.base, TINT_FINAL)
        set_tint(self.overlay, TINT_CLEAR)
        self.busy = false
        return true
    end

    function self:play_signal_swap(target_texture)
        return self:play_screen_transition(target_texture, "console")
    end

    function self:play_screen_transition(target_texture, style)
        if self.busy then
            return false
        end

        local transition_style = style or "console"
        self.busy = true
        set_visible(self.base, true)
        set_visible(self.overlay, true)
        set_texture(self.overlay, target_texture)
        set_tint(self.overlay, TINT_CLEAR)

        if transition_style == "soft" then
            animate_tint(self.overlay, TINT_CLEAR, { 0.72, 0.92, 1.0, 0.42 }, 0.06, 4)
            animate_tint(self.overlay, { 0.72, 0.92, 1.0, 0.42 }, { 0.94, 1.0, 1.0, 0.84 }, 0.08, 5)
            animate_tint(self.overlay, { 0.94, 1.0, 1.0, 0.84 }, TINT_FINAL, 0.08, 5)
        elseif transition_style == "glitch" then
            animate_tint(self.overlay, TINT_CLEAR, { 0.38, 0.88, 1.0, 0.60 }, 0.03, 2)
            animate_tint(self.overlay, { 0.38, 0.88, 1.0, 0.60 }, { 0.82, 1.0, 1.0, 0.28 }, 0.03, 2)
            animate_tint(self.overlay, { 0.82, 1.0, 1.0, 0.28 }, { 0.46, 0.80, 0.92, 0.72 }, 0.04, 3)
            animate_tint(self.overlay, { 0.46, 0.80, 0.92, 0.72 }, TINT_FINAL, 0.09, 5)
        else
            animate_tint(self.overlay, TINT_CLEAR, SWAP_FLASH, 0.05, 4)
            animate_tint(self.overlay, SWAP_FLASH, SWAP_HOLD, 0.06, 4)
            animate_tint(self.overlay, SWAP_HOLD, SWAP_FLICKER, 0.04, 3)
            animate_tint(self.overlay, SWAP_FLICKER, SWAP_SETTLE, 0.05, 3)
            animate_tint(self.overlay, SWAP_SETTLE, TINT_FINAL, 0.08, 5)
        end

        self.current_texture = target_texture
        set_texture(self.base, self.current_texture)
        set_tint(self.base, TINT_FINAL)
        set_tint(self.overlay, TINT_CLEAR)
        self.busy = false
        return true
    end

    return self
end

return ScreenConsoleAnimator
