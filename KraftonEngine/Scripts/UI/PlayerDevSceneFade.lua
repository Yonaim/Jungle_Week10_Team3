local fade_elapsed = 0.0
local fade_duration = 0.42
local flash = nil

local function get_component(name)
    local component = obj:GetComponent(name)
    if component and component:IsValid() then
        return component
    end
    return nil
end

function BeginPlay()
    flash = get_component("WhiteFlash")
    if not flash then
        return
    end

    flash:SetVisible(true)
    flash:SetTint(1.0, 1.0, 1.0, 1.0)
end

function Tick(dt)
    if not flash then
        return
    end

    fade_elapsed = fade_elapsed + (dt or 0.0)
    local progress = math.min(fade_elapsed / fade_duration, 1.0)
    local alpha = 1.0 - progress
    flash:SetTint(1.0, 1.0, 1.0, alpha)

    if progress >= 1.0 then
        flash:SetVisible(false)
        flash = nil
    end
end
