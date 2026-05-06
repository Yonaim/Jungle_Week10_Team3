local Engine = require("Common.Engine")
local Math = require("Common.Math")

local UI = {}

function UI.SetVisible(component, visible)
    if Engine.IsValidComponent(component) then
        component:SetVisible(visible)
    end
end

function UI.SetText(component, text)
    if Engine.IsValidComponent(component) then
        component:SetText(text or "")
    end
end

function UI.SetLabel(component, text)
    if Engine.IsValidComponent(component) then
        component:SetLabel(text or "")
    end
end

function UI.SetTexture(component, texture_path)
    if Engine.IsValidComponent(component) and texture_path and texture_path ~= "" then
        component:SetTexture(texture_path)
    end
end

function UI.GetCachedComponent(cache, name)
    if type(cache) ~= "table" then
        return nil
    end

    return cache[name]
end

function UI.SetCachedVisible(cache, name, visible)
    UI.SetVisible(UI.GetCachedComponent(cache, name), visible)
end

function UI.SetCachedText(cache, name, text)
    UI.SetText(UI.GetCachedComponent(cache, name), text)
end

function UI.SetCachedTint(cache, name, r, g, b, a)
    UI.SetTint(UI.GetCachedComponent(cache, name), r, g, b, a)
end

function UI.SetCachedTexture(cache, name, texture_path)
    UI.SetTexture(UI.GetCachedComponent(cache, name), texture_path)
end

function UI.SetTint(component, r, g, b, a)
    if not Engine.IsValidComponent(component) then
        return
    end

    if type(r) == "table" then
        component:SetTint(r[1], r[2], r[3], r[4])
        return
    end

    component:SetTint(r, g, b, a)
end

function UI.SetScreenPosition(component, x, y, z)
    if Engine.IsValidComponent(component) then
        component:SetScreenPositionXYZ(x, y, z or 0.0)
    end
end

function UI.SetScreenSize(component, width, height, z)
    if Engine.IsValidComponent(component) then
        component:SetScreenSizeXYZ(width, height, z or 0.0)
    end
end

function UI.LerpTint(from_tint, to_tint, t)
    local alpha = Math.Clamp01(t)

    return {
        Math.Lerp(from_tint[1], to_tint[1], alpha),
        Math.Lerp(from_tint[2], to_tint[2], alpha),
        Math.Lerp(from_tint[3], to_tint[3], alpha),
        Math.Lerp(from_tint[4], to_tint[4], alpha),
    }
end

function UI.AnimateTint(component, from_tint, to_tint, duration, steps)
    if not Engine.IsValidComponent(component) then
        return
    end

    local frame_count = steps or 5
    if duration <= 0.0 or frame_count <= 0 then
        UI.SetTint(component, to_tint)
        return
    end

    local step_duration = duration / frame_count
    local index = 1
    while index <= frame_count do
        local t = index / frame_count
        UI.SetTint(component, UI.LerpTint(from_tint, to_tint, t))
        wait(step_duration)
        index = index + 1
    end
end

return UI
