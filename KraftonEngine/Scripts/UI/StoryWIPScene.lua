local ScreenConsoleAnimator = require("UI.ScreenConsoleAnimator")

local TEXTURES = {
    off = "Asset/Content/Texture/Story/control_room_screen_off.png",
    on = "Asset/Content/Texture/Story/control_room_screen_on.png",
    baek = "Asset/Content/Texture/Story/control_room_screen_on_baek.png",
    lim = "Asset/Content/Texture/Story/control_room_screen_on_lim.png",
    baek_lim = "Asset/Content/Texture/Story/control_room_screen_on_baek_lim.png",
}

local ui = {}
local animator = nil
local preview_busy = false
local current_mode = "off"
local current_texture = TEXTURES.off

local function get_component(name)
    local component = obj:GetComponent(name)
    if component and component:IsValid() then
        return component
    end

    debug_log("StoryWIPScene component missing:", name)
    return nil
end

local function cache_component(name)
    ui[name] = get_component(name)
end

local function set_visible(name, visible)
    local component = ui[name]
    if component then
        component:SetVisible(visible)
    end
end

local function set_text(name, text)
    local component = ui[name]
    if component then
        component:SetText(text)
    end
end

local function set_tint(name, r, g, b, a)
    local component = ui[name]
    if component then
        component:SetTint(r, g, b, a)
    end
end

local function reset_overlay_ui()
    set_visible("Logo", false)
    set_visible("CommsPanel", false)
    set_visible("Portrait", false)
    set_visible("DebugIconGrid", false)
    set_visible("DebugIconWire", false)
    set_visible("DebugIconGizmo", false)
    set_visible("DebugIconPanel", false)
    set_visible("RedOverlay", false)
    set_visible("WhiteFlash", false)
    set_visible("BootHeader", false)
    set_visible("BootLine1", false)
    set_visible("BootLine2", false)
    set_visible("BootLine3", false)
    set_visible("BootLine4", false)
    set_visible("NarrationLine1", false)
    set_visible("NarrationLine2", false)
    set_visible("NarrationLine3", false)
    set_visible("NarrationLine4", false)
    set_visible("NarrationLine5", false)
    set_visible("NarrationLine6", false)
    set_visible("SystemLine1", false)
    set_visible("SystemLine2", false)
    set_visible("SystemLine3", false)
    set_visible("SystemLine4", false)
    set_visible("SpeakerName", false)
    set_visible("DialogueLine1", false)
    set_visible("DialogueLine2", false)
    set_visible("DialogueLine3", false)
    set_visible("DialogueLine4", false)
    set_visible("MissionTitle", false)
    set_visible("MissionLine1", false)
    set_visible("MissionLine2", false)
    set_visible("MissionLine3", false)
    set_visible("MissionLine4", false)
    set_visible("MissionLine5", false)
    set_visible("HudLine1", false)
    set_visible("HudLine2", false)
    set_visible("HudLine3", false)
    set_visible("HudLine4", false)
    set_visible("CountdownText", false)
end

local function prepare_preview()
    reset_overlay_ui()

    set_visible("BgBase", false)
    set_visible("BgTexture", true)
    set_visible("PowerGlow", true)

    set_text("SkipHint", "[SPACE] REPLAY  [B] BAEK  [L] LIM  [N] BAEK+LIM")
    set_tint("SkipHint", 0.55, 0.72, 0.95, 0.9)
    set_visible("SkipHint", true)

    set_text("StatusText", "")
    set_visible("StatusText", true)
    set_tint("StatusText", 0.58, 0.88, 1.0, 1.0)

    animator:reset_to_off(TEXTURES.off, TEXTURES.on)
    current_mode = "off"
    current_texture = TEXTURES.off
end

local function play_screen_texture(target_texture, status_text, style, mode_name)
    if not animator or preview_busy or not target_texture or target_texture == "" then
        return false
    end

    if status_text and status_text ~= "" then
        set_text("StatusText", status_text)
    end

    animator:play_screen_transition(target_texture, style or "console")
    current_texture = target_texture
    current_mode = mode_name or current_mode
    return true
end

function RunPreviewSequence()
    if not animator or preview_busy then
        return
    end

    preview_busy = true
    prepare_preview()

    set_text("StatusText", "CONTROL ROOM SCREEN POWER ON")
    animator:play_power_on(TEXTURES.on)
    current_mode = "on"

    set_text("StatusText", "SCREEN ONLINE")
    wait(0.75)

    play_screen_texture(TEXTURES.baek, "SIGNAL PATCH: BAEK CHANNEL", "console", "baek")

    set_text("StatusText", "BAEK FEED LOCKED")
    preview_busy = false
end

function RunBaekOnly()
    if not animator or preview_busy or current_mode == "off" then
        return
    end

    preview_busy = true
    play_screen_texture(TEXTURES.baek, "SIGNAL PATCH: BAEK CHANNEL", "console", "baek")
    set_text("StatusText", "BAEK FEED LOCKED")
    preview_busy = false
end

function RunLimOnly()
    if not animator or preview_busy or current_mode == "off" then
        return
    end

    preview_busy = true
    play_screen_texture(TEXTURES.lim, "SIGNAL PATCH: LIM CHANNEL", "soft", "lim")
    set_text("StatusText", "LIM FEED LOCKED")
    preview_busy = false
end

function RunBaekLimOnly()
    if not animator or preview_busy or current_mode == "off" then
        return
    end

    preview_busy = true
    play_screen_texture(TEXTURES.baek_lim, "COMPOSITE FEED: BAEK + LIM", "glitch", "baek_lim")
    set_text("StatusText", "COMPOSITE FEED LOCKED")
    preview_busy = false
end

function BeginPlay()
    local names = {
        "BgBase",
        "BgTexture",
        "PowerGlow",
        "RedOverlay",
        "WhiteFlash",
        "Logo",
        "CommsPanel",
        "Portrait",
        "DebugIconGrid",
        "DebugIconWire",
        "DebugIconGizmo",
        "DebugIconPanel",
        "BootHeader",
        "BootLine1",
        "BootLine2",
        "BootLine3",
        "BootLine4",
        "NarrationLine1",
        "NarrationLine2",
        "NarrationLine3",
        "NarrationLine4",
        "NarrationLine5",
        "NarrationLine6",
        "SystemLine1",
        "SystemLine2",
        "SystemLine3",
        "SystemLine4",
        "SpeakerName",
        "DialogueLine1",
        "DialogueLine2",
        "DialogueLine3",
        "DialogueLine4",
        "MissionTitle",
        "MissionLine1",
        "MissionLine2",
        "MissionLine3",
        "MissionLine4",
        "MissionLine5",
        "HudLine1",
        "HudLine2",
        "HudLine3",
        "HudLine4",
        "CountdownText",
        "StatusText",
        "SkipHint",
    }

    for _, name in ipairs(names) do
        cache_component(name)
    end

    animator = ScreenConsoleAnimator.new(ui["BgTexture"], ui["PowerGlow"])
    prepare_preview()
    StartCoroutine("RunPreviewSequence")
end

function Tick(dt)
    if preview_busy then
        return
    end

    if GetKeyDown("SPACE") then
        StartCoroutine("RunPreviewSequence")
    elseif GetKeyDown("B") then
        StartCoroutine("RunBaekOnly")
    elseif GetKeyDown("L") then
        StartCoroutine("RunLimOnly")
    elseif GetKeyDown("N") then
        StartCoroutine("RunBaekLimOnly")
    end
end
