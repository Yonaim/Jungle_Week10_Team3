DeclareProperties({
    ScenarioPath = { type = "string", default = "Asset/Content/Data/Scenarios/intro.scenario.json" },
    NextScene = { type = "string", default = "playerdev.scene" },
    AutoAdvanceDelay = { type = "float", default = 3.0, min = 0.0, max = 10.0 },
})

local ScenarioLoader = require("UI.ScenarioLoader")

local SCENARIO_PATH = property("ScenarioPath", "Asset/Content/Data/Scenarios/intro.scenario.json")
local NEXT_SCENE = property("NextScene", "playerdev.scene")
local AUTO_ADVANCE_DELAY = property("AutoAdvanceDelay", 3.0)

local ui = {}
local intro_finished = false
local scenario = nil
local IMMEDIATE_SCENE = "PlayerDev.Scene"

local SPEAKER_COLORS = {
    BAEK_COMMANDER = { 0.62, 0.86, 1.0 },
    LIM_COMMANDER = { 1.0, 0.77, 0.35 },
}

local STYLE_COLORS = {
    debug_blue = { 0.40, 0.78, 1.0, 1.0 },
    error_red = { 1.0, 0.34, 0.34, 1.0 },
    hud = { 0.55, 0.86, 1.0, 1.0 },
    title = { 0.58, 0.88, 1.0, 1.0 },
    center_text = { 0.93, 0.97, 1.0, 1.0 },
}

local function get_component(name)
    local component = obj:GetComponent(name)
    if component and component:IsValid() then
        return component
    end

    warn("StoryScene component missing:", name)
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

local function set_texture(name, texture_path)
    local component = ui[name]
    if component and texture_path and texture_path ~= "" then
        component:SetTexture(texture_path)
    end
end

local function hide_lines(prefix, count)
    local index = 1
    while index <= count do
        local name = prefix .. tostring(index)
        set_text(name, "")
        set_visible(name, false)
        index = index + 1
    end
end

local function set_line_group(prefix, count, lines, r, g, b, a)
    local index = 1
    while index <= count do
        local name = prefix .. tostring(index)
        local text = lines[index]
        if text and text ~= "" then
            set_text(name, text)
            set_tint(name, r, g, b, a)
            set_visible(name, true)
        else
            set_text(name, "")
            set_visible(name, false)
        end
        index = index + 1
    end
end

local function clear_all_text()
    hide_lines("BootLine", 4)
    hide_lines("NarrationLine", 6)
    hide_lines("SystemLine", 4)
    hide_lines("DialogueLine", 4)
    hide_lines("MissionLine", 5)
    hide_lines("HudLine", 4)

    set_text("BootHeader", "")
    set_visible("BootHeader", false)
    set_text("SpeakerName", "")
    set_visible("SpeakerName", false)
    set_text("MissionTitle", "")
    set_visible("MissionTitle", false)
    set_text("CountdownText", "")
    set_visible("CountdownText", false)
    set_text("StatusText", "")
    set_visible("StatusText", false)
end

local function hide_overlays()
    set_visible("Logo", false)
    set_visible("CommsPanel", false)
    set_visible("Portrait", false)
    set_visible("DebugIconGrid", false)
    set_visible("DebugIconWire", false)
    set_visible("DebugIconGizmo", false)
    set_visible("DebugIconPanel", false)
    set_visible("RedOverlay", false)
    set_visible("WhiteFlash", false)
end

local function reset_scene()
    hide_overlays()
    clear_all_text()

    set_texture("BgTexture", "Asset/Content/Texture/background/title_background.png")
    set_texture("Logo", "Asset/Content/Texture/UI/logo_techlab_color.png")
    set_texture("Portrait", "Asset/Content/Texture/Story/profile_baek.png")

    set_visible("BgBase", true)
    set_visible("BgTexture", true)
    set_tint("BgBase", 0.01, 0.02, 0.04, 1.0)
    set_tint("BgTexture", 0.05, 0.09, 0.16, 0.32)
    set_tint("RedOverlay", 1.0, 0.1, 0.1, 0.0)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 0.0)
    set_tint("Logo", 1.0, 1.0, 1.0, 0.0)
    set_tint("CommsPanel", 0.72, 0.9, 1.0, 0.95)
    set_tint("Portrait", 1.0, 1.0, 1.0, 1.0)

    set_text("SkipHint", "[ESC] SKIP  [SPACE] NEXT")
    set_tint("SkipHint", 0.55, 0.72, 0.95, 0.9)
    set_visible("SkipHint", true)
end

local function style_color(style, fallback)
    local value = STYLE_COLORS[style]
    if value then
        return value[1], value[2], value[3], value[4]
    end
    return fallback[1], fallback[2], fallback[3], fallback[4]
end

local function speaker_color(speaker)
    local value = SPEAKER_COLORS[speaker]
    if value then
        return value[1], value[2], value[3]
    end
    return 1.0, 1.0, 1.0
end

local function wait_for_input()
    while not intro_finished do
        if GetKeyDown("SPACE") then
            return
        end
        wait_frames(1)
    end
end

local function wait_for_advance(mode, fallback_seconds)
    if mode == "input" then
        wait_for_input()
        return
    end

    if fallback_seconds and fallback_seconds > 0.0 then
        wait(fallback_seconds)
    end
end

local function play_named_sound(sound_name, looping)
    if not scenario or not sound_name or sound_name == "" then
        return
    end

    local resolved = ScenarioLoader.resolve_asset(scenario, "sounds", sound_name)
    if resolved and resolved ~= "" then
        play_sfx(resolved, looping == true)
    end
end

local function show_dialogue(step)
    local lines = ScenarioLoader.dialogue_lines(step, 4)
    local portrait_path = ScenarioLoader.resolve_dialogue_texture(scenario, step.speaker, step.emotion)
    local speaker_name = ScenarioLoader.resolve_speaker_name(scenario, step.speaker)
    local red, green, blue = speaker_color(step.speaker)

    set_visible("CommsPanel", true)
    set_visible("Portrait", portrait_path ~= nil)
    if portrait_path then
        set_texture("Portrait", portrait_path)
    end

    set_text("SpeakerName", speaker_name)
    set_tint("SpeakerName", red, green, blue, 1.0)
    set_visible("SpeakerName", true)

    set_line_group("DialogueLine", 4, lines, 0.95, 0.97, 1.0, 1.0)
    wait_for_advance(step.advance, 1.8)
end

local function show_system_log(step)
    local lines = ScenarioLoader.line_array(step, "lines")
    local red, green, blue, alpha = style_color(step.style, { 0.40, 0.78, 1.0, 1.0 })
    local target_prefix = "SystemLine"
    local target_count = 4

    if step.id and step.id:find("boot", 1, true) then
        target_prefix = "BootLine"
        set_text("BootHeader", "JUNGLE TECH LAB INTERNAL ENGINE")
        set_tint("BootHeader", 0.54, 0.86, 1.0, 1.0)
        set_visible("BootHeader", true)
    end

    if step.style == "hud" then
        target_prefix = "HudLine"
    end

    set_line_group(target_prefix, target_count, lines, red, green, blue, alpha)
    wait_for_advance(step.advance, (step.interval or 0.25) * math.max(#lines, 1))
end

local function show_narration(step)
    local lines = ScenarioLoader.line_array(step, "lines")
    local red, green, blue, alpha = style_color(step.style, { 0.93, 0.97, 1.0, 1.0 })
    set_line_group("NarrationLine", 6, lines, red, green, blue, alpha)
    wait_for_advance(step.advance, 1.4)
end

local function show_mission(step)
    local lines = {}
    local items = step.items or {}
    local index = 1
    while index <= #items and index <= 5 do
        lines[index] = tostring(index) .. ". " .. tostring(items[index])
        index = index + 1
    end

    set_text("MissionTitle", step.title or "MISSION OBJECTIVE")
    set_tint("MissionTitle", 1.0, 0.92, 0.58, 1.0)
    set_visible("MissionTitle", true)
    set_line_group("MissionLine", 5, lines, 0.88, 0.95, 1.0, 1.0)
    wait_for_advance(step.advance, 1.6)
end

local function show_countdown(step)
    local values = step.values or {}
    set_text("StatusText", step.title or "SEQUENCE START")
    set_tint("StatusText", 0.58, 0.88, 1.0, 1.0)
    set_visible("StatusText", true)

    local index = 1
    while index <= #values do
        set_text("CountdownText", tostring(values[index]))
        set_tint("CountdownText", 1.0, 1.0, 1.0, 1.0)
        set_visible("CountdownText", true)
        if step.sounds and step.sounds[1] then
            play_named_sound(step.sounds[1], false)
        end
        wait(step.interval or 1.0)
        index = index + 1
    end
end

local function show_image(step)
    local texture_path = ScenarioLoader.resolve_asset(scenario, "images", step.image)
    if not texture_path then
        return
    end

    if step.target == "background" then
        set_texture("BgTexture", texture_path)
        set_visible("BgTexture", true)
        set_tint("BgTexture", 1.0, 1.0, 1.0, 1.0)
    else
        set_texture("Logo", texture_path)
        set_visible("Logo", true)
        set_tint("Logo", 1.0, 1.0, 1.0, 1.0)
    end

    local total_hold = (step.duration or 0.0) + (step.hold or 0.0)
    if total_hold > 0.0 then
        wait(total_hold)
    end
end

local function show_effect(step)
    if step.sounds and step.sounds[1] then
        play_named_sound(step.sounds[1], false)
    end

    if step.name == "glitch" then
        set_visible("RedOverlay", true)
        set_tint("RedOverlay", 0.95, 0.12, 0.12, 0.18)
        wait(step.duration or 0.5)
        set_tint("RedOverlay", 0.95, 0.12, 0.12, 0.0)
    else
        wait(step.duration or 0.3)
    end
end

local function show_flash(step)
    set_visible("WhiteFlash", true)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 1.0)
    wait(step.duration or 0.35)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 0.0)
end

local function show_montage(step)
    set_visible("DebugIconGrid", true)
    set_visible("DebugIconWire", true)
    set_visible("DebugIconGizmo", true)
    set_visible("DebugIconPanel", true)

    local texture_path = ScenarioLoader.resolve_asset(scenario, "images", step.image)
    if texture_path then
        set_texture("BgTexture", texture_path)
        set_visible("BgTexture", true)
        set_tint("BgTexture", 1.0, 1.0, 1.0, 0.95)
    end

    wait(step.duration or 1.0)
end

local function show_scene_color(step)
    if step.color == "black" then
        set_tint("BgBase", 0.0, 0.0, 0.0, 1.0)
        set_tint("BgTexture", 0.0, 0.0, 0.0, 0.0)
    elseif step.color == "white" then
        set_tint("BgBase", 1.0, 1.0, 1.0, 1.0)
        set_tint("BgTexture", 1.0, 1.0, 1.0, 0.0)
    end

    if step.duration and step.duration > 0.0 then
        wait(step.duration)
    end
end

local function handle_change_scene(step)
    if step.scene == "play" then
        finish_intro()
        return
    end

    finish_intro(step.scene)
end

function finish_intro(target_scene)
    if intro_finished then
        return
    end

    intro_finished = true
    stop_bgm()
    load_scene(target_scene or NEXT_SCENE)
end

local function load_playerdev_immediately()
    finish_intro(IMMEDIATE_SCENE)
end

local function run_step(step)
    if intro_finished or type(step) ~= "table" then
        return
    end

    if step.type == "screen" then
        show_scene_color(step)
    elseif step.type == "sound" then
        play_named_sound(step.name, false)
    elseif step.type == "systemLog" then
        show_system_log(step)
    elseif step.type == "showImage" then
        show_image(step)
    elseif step.type == "narration" then
        show_narration(step)
    elseif step.type == "montage" then
        show_montage(step)
    elseif step.type == "effect" then
        show_effect(step)
    elseif step.type == "dialogue" then
        show_dialogue(step)
    elseif step.type == "wait" then
        wait(step.duration or 0.0)
    elseif step.type == "missionObjective" then
        show_mission(step)
    elseif step.type == "countdown" then
        show_countdown(step)
    elseif step.type == "flash" then
        show_flash(step)
    elseif step.type == "changeScene" then
        handle_change_scene(step)
    elseif step.type == "showPortrait" then
        local portrait_path = ScenarioLoader.resolve_dialogue_texture(scenario, step.speaker, step.emotion)
        if portrait_path then
            set_texture("Portrait", portrait_path)
            set_visible("Portrait", true)
            set_visible("CommsPanel", true)
        end
        if step.sounds and step.sounds[1] then
            play_named_sound(step.sounds[1], false)
        end
    end
end

function RunIntro()
    reset_scene()
    stop_bgm()
    play_bgm("Game.Sound.Background.Cutscene1", false)

    scenario = ScenarioLoader.load(SCENARIO_PATH, load_json_file)
    if not scenario then
        warn("Failed to load scenario:", SCENARIO_PATH)
        finish_intro()
        return
    end

    local steps = scenario.sequence or {}
    local index = 1
    while index <= #steps and not intro_finished do
        run_step(steps[index])
        index = index + 1
    end

    if not intro_finished then
        local elapsed = 0.0
        local blink_visible = true
        while elapsed < AUTO_ADVANCE_DELAY and not intro_finished do
            if blink_visible then
                set_tint("StatusText", 0.58, 0.88, 1.0, 1.0)
                set_tint("CountdownText", 1.0, 1.0, 1.0, 1.0)
            else
                set_tint("StatusText", 0.58, 0.88, 1.0, 0.25)
                set_tint("CountdownText", 1.0, 1.0, 1.0, 0.25)
            end

            blink_visible = not blink_visible
            wait(0.3)
            elapsed = elapsed + 0.3
        end

        finish_intro()
    end
end

function BeginPlay()
    play_sfx("Sound.SFX.windows.98.startup", false)

    local names = {
        "BgBase",
        "BgTexture",
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

    load_playerdev_immediately()
end

function Tick(dt)
    if intro_finished then
        return
    end
end
