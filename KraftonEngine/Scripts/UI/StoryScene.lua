DeclareProperties({
    ScenarioPath = { type = "string", default = "Asset/Content/Data/Scenarios/story.json" },
    NextScene = { type = "string", default = "playerdev.scene" },
})

local ScenarioLoader = require("UI.ScenarioLoader")

local SCENARIO_PATH = property("ScenarioPath", "Asset/Content/Data/Scenarios/story.json")
local NEXT_SCENE = property("NextScene", "playerdev.scene")

local ui = {}
local scenario = nil
local pages = {}
local current_index = 1
local input_cooldown = 0.0
local auto_advance_timer = nil
local scene_change_timer = nil
local scene_change_duration = 0.18
local pending_page_index = nil
local current_story_background_path = nil
local story_finished = false
local skip_hold_elapsed = 0.0
local typing_state = {
    active = false,
    chars = {},
    visible_count = 0,
    elapsed = 0.0,
    interval = 0.028,
    full_text = "",
}
local dialogue_background_state = {
    current_one_key = nil,
}
local OVERLAY_COMPONENTS = {
    control_room_baek_0 = "StoryBackgroundBaek0",
    control_room_baek_1 = "StoryBackgroundBaek1",
    control_room_lim_0 = "StoryBackgroundLim0",
    control_room_lim_1 = "StoryBackgroundLim1",
    control_room_pod = "StoryBackgroundPod",
    control_room_baek_lim_0 = "StoryBackgroundBaekLim0",
    control_room_baek_lim_1 = "StoryBackgroundBaekLim1",
}
local dialogue_transition = {
    active = false,
    elapsed = 0.0,
    duration = 0.0,
    mode = nil,
    zero_key = nil,
    zero_path = nil,
    one_key = nil,
    one_path = nil,
}
local pending_dialogue_ui = nil
local prologue_state = {
    mode = nil,
    elapsed = 0.0,
    line_index = 0,
    lines = {},
    intro_fade_seconds = 3.0,
    intro_hold_seconds = 1.5,
    outro_fade_seconds = 0.9,
    control_room_fade_seconds = 0.9,
    crt_lead_seconds = 1.0,
    crt_switch_delay_seconds = 0.2,
    post_crt_hold_seconds = 1.2,
}

local MAX_LINE_WIDTH_UNITS = 28
local SPEAKER_NAME_COLOR = { 134.0 / 255.0, 251.0 / 255.0, 255.0 / 255.0, 233.0 / 255.0 }
local DIALOGUE_TEXT_COLOR = { 235.0 / 255.0, 247.0 / 255.0, 255.0 / 255.0, 209.0 / 255.0 }
local PROLOGUE_TEXT_COLOR = { 235.0 / 255.0, 247.0 / 255.0, 255.0 / 255.0, 0.94 }
local PROLOGUE_HINT_COLOR = { 0.72, 0.84, 1.0, 0.88 }
local PROLOGUE_DIM_BACKGROUND = 0.32
local INPUT_COOLDOWN_SECONDS = 0.18
local DIALOGUE_TRANSITION_DURATION = 0.3
local TYPEWRITER_INTERVAL_SECONDS = 0.028
local NEXT_PAGE_SOUND_PATH = "Asset/Content/Sound/SFX/story_next.mp3"
local TYPEWRITER_SOUND_PATH = "Asset/Content/Sound/SFX/crt-type.wav"
local DEFAULT_BOOT_SOUND_PATH = "Asset/Content/Sound/SFX/windows-98-startup.mp3"
local DEFAULT_CRT_ON_SOUND_PATH = "Asset/Content/Sound/SFX/crt-on.mp3"
local DEFAULT_GO_SOUND_PATH = "Sound.SFX.go"
local TERMINAL_PROMPT = ">_ "
local HOLD_SKIP_SECONDS = 2.0
local HOLD_SKIP_HINT_TEXT = "Hold Space to Skip..."
local apply_page = nil
local resolve_neutral_background = nil

local function split_utf8_chars(value)
    local chars = {}
    if type(value) ~= "string" or value == "" then
        return chars
    end

    local index = 1
    local length = #value

    while index <= length do
        local byte = string.byte(value, index)
        local char_length = 1

        if byte >= 0xF0 then
            char_length = 4
        elseif byte >= 0xE0 then
            char_length = 3
        elseif byte >= 0xC0 then
            char_length = 2
        end

        chars[#chars + 1] = value:sub(index, index + char_length - 1)
        index = index + char_length
    end

    return chars
end

local function is_typing_sound_character(char)
    return char ~= nil and char ~= "" and char ~= " " and char ~= "\n" and char ~= "\t"
end

local function format_terminal_message(text)
    local source = type(text) == "string" and text or ""
    if source == "" then
        return ""
    end

    local function normalize_newlines(value)
        local result = {}
        local index = 1
        local length = #value

        while index <= length do
            local char = value:sub(index, index)
            if char == "\r" then
                result[#result + 1] = "\n"
                if index < length and value:sub(index + 1, index + 1) == "\n" then
                    index = index + 1
                end
            else
                result[#result + 1] = char
            end
            index = index + 1
        end

        return table.concat(result)
    end

    local function is_ascii_space(char)
        return char == " " or char == "\t"
    end

    local function char_width_units(char)
        local byte = string.byte(char, 1)
        if not byte then
            return 0
        end

        if byte < 0x80 then
            if is_ascii_space(char) then
                return 0.45
            end
            return 0.6
        end

        return 1.0
    end

    local function trim_spaces(value)
        local chars = split_utf8_chars(value)
        local start_index = 1
        local end_index = #chars

        while start_index <= end_index and is_ascii_space(chars[start_index]) do
            start_index = start_index + 1
        end

        while end_index >= start_index and is_ascii_space(chars[end_index]) do
            end_index = end_index - 1
        end

        if start_index > end_index then
            return ""
        end

        return table.concat(chars, "", start_index, end_index)
    end

    local function wrap_line(line)
        local source = trim_spaces(line)
        if source == "" then
            return { "" }
        end

        local chars = split_utf8_chars(source)
        local lines = {}
        local current = {}
        local last_space_index = nil
        local current_width = 0.0

        local function current_text(count)
            return table.concat(current, "", 1, count or #current)
        end

        local function push_current(text)
            lines[#lines + 1] = trim_spaces(text)
            current = {}
            last_space_index = nil
            current_width = 0.0
        end

        for _, char in ipairs(chars) do
            current[#current + 1] = char
            current_width = current_width + char_width_units(char)
            if char == " " then
                last_space_index = #current
            end

            if current_width >= MAX_LINE_WIDTH_UNITS then
                if last_space_index and last_space_index > 1 then
                    local split_index = last_space_index
                    local snapshot = current
                    push_current(table.concat(snapshot, "", 1, split_index - 1))

                    local remaining = {}
                    for index = split_index + 1, #snapshot do
                        remaining[#remaining + 1] = snapshot[index]
                    end
                    current = remaining
                    current_width = 0.0
                    last_space_index = nil

                    for index, remaining_char in ipairs(current) do
                        current_width = current_width + char_width_units(remaining_char)
                        if remaining_char == " " then
                            last_space_index = index
                        end
                    end
                else
                    push_current(current_text())
                end
            end
        end

        if #current > 0 then
            push_current(current_text())
        end

        return lines
    end

    local normalized = normalize_newlines(source)
    local wrapped_lines = {}
    local current_line = {}

    local function flush_line()
        local raw_line = table.concat(current_line)
        current_line = {}

        if raw_line == "" and #wrapped_lines > 0 then
            wrapped_lines[#wrapped_lines + 1] = ""
        elseif raw_line ~= "" then
            local line_parts = wrap_line(raw_line)
            for _, line in ipairs(line_parts) do
                wrapped_lines[#wrapped_lines + 1] = line
            end
        end
    end

    local index = 1
    while index <= #normalized do
        local char = normalized:sub(index, index)
        if char == "\n" then
            flush_line()
        else
            current_line[#current_line + 1] = char
        end
        index = index + 1
    end

    if #current_line > 0 then
        flush_line()
    end

    if #wrapped_lines == 0 then
        return ""
    end

    local message = table.concat(wrapped_lines, "\n")
    if message == "" then
        return ""
    end

    local continuation_indent = "   "
    return message:gsub("\n", "\n" .. continuation_indent)
end

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

local function play_story_sfx(sound_path)
    if type(sound_path) == "string" and sound_path ~= "" then
        play_sfx(sound_path, false)
    end
end

local function resolve_story_image(image_key, fallback_path)
    local resolved = ScenarioLoader.resolve_asset(scenario, "images", image_key)
    if resolved and resolved ~= "" then
        return resolved
    end
    return fallback_path
end

local function resolve_story_sound(sound_key, fallback_path)
    local resolved = ScenarioLoader.resolve_asset(scenario, "sounds", sound_key)
    if resolved and resolved ~= "" then
        return resolved
    end
    return fallback_path
end

local function play_story_bgm(sound_path, looping)
    if type(sound_path) == "string" and sound_path ~= "" then
        stop_bgm()
        play_bgm(sound_path, looping == true)
    end
end

local function ease_in_power(value, power)
    local t = math.max(0.0, math.min(1.0, value or 0.0))
    local exponent = power or 4.0
    return math.pow(t, exponent)
end

local function set_story_background_texture(texture_path)
    if not texture_path or texture_path == "" then
        return
    end

    if current_story_background_path == texture_path then
        return
    end

    set_texture("StoryBackground", texture_path)
    current_story_background_path = texture_path
end

local function preload_component_texture(name)
    local component = ui[name]
    if not component or not component.GetTexturePath then
        return
    end

    local texture_path = component:GetTexturePath()
    if not texture_path or texture_path == "" or texture_path == "None" then
        return
    end

    component:SetTexture(texture_path)
end

local function preload_story_background_layers()
    preload_component_texture("StoryBackground")

    for _, component_name in pairs(OVERLAY_COMPONENTS) do
        preload_component_texture(component_name)
    end
end

local function set_default_background()
    local _, neutral_path = resolve_neutral_background()
    if neutral_path then
        set_story_background_texture(neutral_path)
    end
    set_tint("StoryBackground", 1.0, 1.0, 1.0, 1.0)
end

local function set_overlay_alpha(background_key, alpha)
    local component_name = OVERLAY_COMPONENTS[background_key]
    if component_name then
        set_visible(component_name, true)
        set_tint(component_name, 1.0, 1.0, 1.0, alpha or 0.0)
    end
end

local function hide_dialogue_background_layers()
    for background_key, component_name in pairs(OVERLAY_COMPONENTS) do
        set_visible(component_name, true)
        set_tint(component_name, 1.0, 1.0, 1.0, 0.0)
    end
end

local function reset_dialogue_transition()
    dialogue_transition.active = false
    dialogue_transition.elapsed = 0.0
    dialogue_transition.duration = 0.0
    dialogue_transition.mode = nil
    dialogue_transition.zero_key = nil
    dialogue_transition.zero_path = nil
    dialogue_transition.one_key = nil
    dialogue_transition.one_path = nil
end

local function clear_pending_dialogue_ui()
    pending_dialogue_ui = nil
end

local function reset_typewriter()
    typing_state.active = false
    typing_state.chars = {}
    typing_state.visible_count = 0
    typing_state.elapsed = 0.0
    typing_state.interval = TYPEWRITER_INTERVAL_SECONDS
    typing_state.full_text = ""
end

local function hide_dialogue_ui()
    reset_typewriter()
    set_text("SpeakerName", "")
    set_text("DialogueText", "")
    set_text("NarrationText", "")
    set_text("PageHint", HOLD_SKIP_HINT_TEXT)
    set_visible("SpeakerName", false)
    set_visible("DialogueText", false)
    set_visible("NarrationText", false)
    set_visible("PageHint", true)
end

local function finish_story(target_scene)
    if story_finished then
        return
    end

    story_finished = true
    stop_bgm()
    load_scene(target_scene or NEXT_SCENE)
end

local function start_scene_change_flash()
    scene_change_duration = 2.0
    scene_change_timer = scene_change_duration
    set_visible("WhiteFlash", true)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 0.0)
end

local function is_skip_key_held()
    return GetKey("SPACE") or GetKey("Space")
end

local function update_skip_hold(dt)
    if scene_change_timer or story_finished then
        skip_hold_elapsed = 0.0
        return false
    end

    if is_skip_key_held() then
        skip_hold_elapsed = skip_hold_elapsed + (dt or 0.0)
        if skip_hold_elapsed >= HOLD_SKIP_SECONDS then
            finish_story()
            return true
        end
    else
        skip_hold_elapsed = 0.0
    end

    return false
end

local function set_dialogue_alpha(alpha)
    local clamped = math.max(0.0, math.min(1.0, alpha or 0.0))
    set_tint("NarrationText", PROLOGUE_TEXT_COLOR[1], PROLOGUE_TEXT_COLOR[2], PROLOGUE_TEXT_COLOR[3], PROLOGUE_TEXT_COLOR[4] * clamped)
    set_tint("PageHint", PROLOGUE_HINT_COLOR[1], PROLOGUE_HINT_COLOR[2], PROLOGUE_HINT_COLOR[3], PROLOGUE_HINT_COLOR[4] * clamped)
end

local function build_prologue_text(line_count)
    local lines = {}
    local visible_count = math.min(line_count or 0, #prologue_state.lines)

    for index = 1, visible_count do
        lines[#lines + 1] = tostring(prologue_state.lines[index] or "")
    end

    return table.concat(lines, "\n")
end

local function show_prologue_lines()
    set_visible("SpeakerName", false)
    set_visible("DialogueText", false)
    set_visible("NarrationText", true)
    set_visible("PageHint", true)
    set_text("SpeakerName", "")
    set_text("NarrationText", build_prologue_text(prologue_state.line_index))
    set_text("PageHint", HOLD_SKIP_HINT_TEXT)
    set_tint("StoryBackground", PROLOGUE_DIM_BACKGROUND, PROLOGUE_DIM_BACKGROUND, PROLOGUE_DIM_BACKGROUND, 1.0)
    set_dialogue_alpha(1.0)
end

local function update_typewriter_text()
    if not typing_state.full_text or typing_state.full_text == "" then
        set_text("DialogueText", TERMINAL_PROMPT)
        return
    end

    if typing_state.visible_count <= 0 then
        set_text("DialogueText", TERMINAL_PROMPT)
        return
    end

    set_text("DialogueText", TERMINAL_PROMPT .. table.concat(typing_state.chars, "", 1, typing_state.visible_count))
end

local function complete_typewriter()
    if typing_state.full_text == "" then
        reset_typewriter()
        set_text("DialogueText", TERMINAL_PROMPT)
        return
    end

    typing_state.active = false
    typing_state.visible_count = #typing_state.chars
    update_typewriter_text()
end

local function start_typewriter(text)
    reset_typewriter()
    typing_state.full_text = type(text) == "string" and text or ""
    typing_state.chars = split_utf8_chars(typing_state.full_text)

    if #typing_state.chars == 0 then
        set_text("DialogueText", TERMINAL_PROMPT)
        return
    end

    typing_state.active = true
    typing_state.interval = TYPEWRITER_INTERVAL_SECONDS
    update_typewriter_text()
end

local function show_dialogue_ui(page)
    local speaker_name = ScenarioLoader.resolve_speaker_name(scenario, page.speaker)
    local message = format_terminal_message(tostring(page.message or ""))

    set_visible("SpeakerName", true)
    set_visible("DialogueText", true)
    set_visible("NarrationText", false)
    set_visible("PageHint", true)
    set_text("SpeakerName", speaker_name)
    set_tint("SpeakerName", SPEAKER_NAME_COLOR[1], SPEAKER_NAME_COLOR[2], SPEAKER_NAME_COLOR[3], SPEAKER_NAME_COLOR[4])
    set_tint("DialogueText", DIALOGUE_TEXT_COLOR[1], DIALOGUE_TEXT_COLOR[2], DIALOGUE_TEXT_COLOR[3], DIALOGUE_TEXT_COLOR[4])
    set_text("PageHint", HOLD_SKIP_HINT_TEXT)
    set_text("DialogueText", TERMINAL_PROMPT)
    start_typewriter(message)
end

local function resolve_page_background(page)
    if not scenario or type(page) ~= "table" then
        return nil
    end

    if type(page.background) == "string" then
        local resolved = ScenarioLoader.resolve_asset(scenario, "images", page.background)
        if resolved then
            return resolved
        end
        if page.background:sub(1, 6) == "Asset/" then
            return page.background
        end
    end

    return nil
end

resolve_neutral_background = function()
    if not scenario then
        return nil, nil
    end

    local neutral_key = "control_room_on"
    local neutral_path = ScenarioLoader.resolve_asset(scenario, "images", neutral_key)
    return neutral_key, neutral_path
end

local function resolve_fade_source_background(page)
    if not scenario or type(page) ~= "table" or type(page.background) ~= "string" then
        return nil, nil
    end

    local background_key = page.background
    if background_key:sub(-2) ~= "_1" then
        return nil, nil
    end

    local base_key = background_key:sub(1, -3) .. "_0"
    local base_path = ScenarioLoader.resolve_asset(scenario, "images", base_key)
    if not base_path then
        return nil, nil
    end

    return base_key, base_path
end

local function complete_dialogue_transition()
    local completed_mode = dialogue_transition.mode
    if completed_mode == "enter" then
        set_overlay_alpha(dialogue_transition.zero_key, 1.0)
        set_overlay_alpha(dialogue_transition.one_key, 1.0)
        dialogue_background_state.current_one_key = dialogue_transition.one_key
        show_dialogue_ui(pending_dialogue_ui)
        clear_pending_dialogue_ui()
    elseif completed_mode == "exit" then
        hide_dialogue_background_layers()
        dialogue_background_state.current_one_key = nil
    end

    reset_dialogue_transition()

    if completed_mode == "exit" and pending_page_index then
        current_index = pending_page_index
        pending_page_index = nil
        apply_page(pages[current_index])
    end
end

local function set_background_immediately(background_key, background_path)
    if not background_path then
        return
    end

    reset_dialogue_transition()
    clear_pending_dialogue_ui()
    set_default_background()
    hide_dialogue_background_layers()
    if background_key then
        set_overlay_alpha(background_key, 1.0)
    end

    dialogue_background_state.current_one_key = background_key
end

local function set_non_dialogue_background(background_path)
    reset_dialogue_transition()
    clear_pending_dialogue_ui()
    set_default_background()
    hide_dialogue_background_layers()
    if type(background_path) == "string" then
        for background_key, component_name in pairs(OVERLAY_COMPONENTS) do
            local resolved = ScenarioLoader.resolve_asset(scenario, "images", background_key)
            if resolved == background_path then
                set_overlay_alpha(background_key, 1.0)
                break
            end
        end
    end
    set_visible("StoryBackground", true)
    set_tint("StoryBackground", 1.0, 1.0, 1.0, 1.0)
    dialogue_background_state.current_one_key = nil
end

local function begin_dialogue_transition(mode, zero_key, zero_path, one_key, one_path)
    if not zero_path or not one_path then
        if mode == "enter" then
            set_background_immediately(one_key, one_path)
        else
            set_non_dialogue_background(select(2, resolve_neutral_background()))
        end
        return
    end

    set_default_background()
    hide_dialogue_background_layers()
    if zero_key then
        set_overlay_alpha(zero_key, 1.0)
    end

    if mode == "enter" then
        if one_key then
            set_overlay_alpha(one_key, 0.0)
        end
    else
        if one_key then
            set_overlay_alpha(one_key, 1.0)
        end
    end

    dialogue_transition.active = true
    dialogue_transition.elapsed = 0.0
    dialogue_transition.duration = DIALOGUE_TRANSITION_DURATION
    dialogue_transition.mode = mode
    dialogue_transition.zero_key = zero_key
    dialogue_transition.zero_path = zero_path
    dialogue_transition.one_key = one_key
    dialogue_transition.one_path = one_path
end

local function is_transition_dialogue_page(page)
    return type(page) == "table"
        and page.type == "dialogue"
        and type(page.background) == "string"
        and page.background:sub(-2) == "_1"
end

local function start_dialogue_intro_transition(page, target_path)
    local base_key, base_path = resolve_fade_source_background(page)
    if not base_path or not target_path then
        set_background_immediately(page.background, target_path)
        return
    end

    begin_dialogue_transition("enter", base_key, base_path, page.background, target_path)
end

local function start_dialogue_exit_transition(page)
    local base_key, base_path = resolve_fade_source_background(page)
    local one_path = resolve_page_background(page)
    if not one_path or not base_path then
        set_non_dialogue_background(select(2, resolve_neutral_background()))
        if pending_page_index then
            current_index = pending_page_index
            pending_page_index = nil
            apply_page(pages[current_index])
        end
        return
    end

    begin_dialogue_transition("exit", base_key, base_path, page.background, one_path)
end

local function is_auto_page(page)
    return type(page) == "table"
        and (page.type == "image" or page.type == "splash")
        and type(page.duration) == "number"
        and page.duration > 0.0
end

local function start_story_sequence()
    prologue_state.mode = "story"
    prologue_state.elapsed = 0.0
    prologue_state.line_index = 0
    set_tint("StoryBackground", 1.0, 1.0, 1.0, 1.0)
    set_visible("StoryBackground", true)
    hide_dialogue_background_layers()
    play_story_bgm(resolve_story_sound("stage_intro", "Asset/Content/Sound/Background/06. Stage Intro.mp3"), true)
    apply_page(pages[current_index])
end

local function start_story_prologue()
    local prologue = type(scenario) == "table" and scenario.prologue or nil
    local lines = type(prologue) == "table" and prologue.lines or nil
    if type(lines) ~= "table" or #lines == 0 then
        start_story_sequence()
        return
    end

    prologue_state.lines = lines
    prologue_state.line_index = 0
    prologue_state.elapsed = 0.0
    prologue_state.mode = "intro_fade_in"
    prologue_state.intro_fade_seconds = (type(prologue.introFadeSeconds) == "number" and prologue.introFadeSeconds > 0.0) and prologue.introFadeSeconds or 3.0
    prologue_state.intro_hold_seconds = (type(prologue.introHoldSeconds) == "number" and prologue.introHoldSeconds >= 0.0) and prologue.introHoldSeconds or 1.5
    prologue_state.outro_fade_seconds = (type(prologue.outroFadeSeconds) == "number" and prologue.outroFadeSeconds > 0.0) and prologue.outroFadeSeconds or 0.9
    prologue_state.control_room_fade_seconds = (type(prologue.controlRoomFadeSeconds) == "number" and prologue.controlRoomFadeSeconds > 0.0) and prologue.controlRoomFadeSeconds or 0.9
    prologue_state.crt_lead_seconds = (type(prologue.crtLeadSeconds) == "number" and prologue.crtLeadSeconds >= 0.0) and prologue.crtLeadSeconds or 1.0
    prologue_state.crt_switch_delay_seconds = (type(prologue.crtSwitchDelaySeconds) == "number" and prologue.crtSwitchDelaySeconds >= 0.0) and prologue.crtSwitchDelaySeconds or 0.2
    prologue_state.post_crt_hold_seconds = (type(prologue.postCrtHoldSeconds) == "number" and prologue.postCrtHoldSeconds >= 0.0) and prologue.postCrtHoldSeconds or 1.2

    stop_bgm()
    reset_typewriter()
    clear_pending_dialogue_ui()
    hide_dialogue_background_layers()
    set_texture("StoryBackground", resolve_story_image("introduce", "Asset/Content/Texture/Story/introduce.png"))
    current_story_background_path = nil
    set_visible("StoryBackground", true)
    set_tint("StoryBackground", 0.0, 0.0, 0.0, 1.0)
    set_visible("SpeakerName", false)
    set_visible("DialogueText", false)
    set_visible("NarrationText", false)
    set_visible("PageHint", false)
    set_text("SpeakerName", "")
    set_text("DialogueText", "")
    set_text("NarrationText", "")
    set_text("PageHint", "")
    play_story_bgm(resolve_story_sound("boot_startup", DEFAULT_BOOT_SOUND_PATH), false)
end

local function try_advance_prologue()
    if input_cooldown > 0.0 then
        return false
    end

    if prologue_state.mode ~= "line_reveal" then
        return false
    end

    input_cooldown = INPUT_COOLDOWN_SECONDS
    play_story_sfx(NEXT_PAGE_SOUND_PATH)

    if prologue_state.line_index < #prologue_state.lines then
        prologue_state.line_index = prologue_state.line_index + 1
        show_prologue_lines()
        if prologue_state.line_index >= #prologue_state.lines then
            prologue_state.mode = "line_complete_hold"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    return false
end

local function update_prologue(dt)
    local delta = dt or 0.0

    if prologue_state.mode == "intro_fade_in" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local raw_progress = math.min(prologue_state.elapsed / prologue_state.intro_fade_seconds, 1.0)
        local progress = ease_in_power(raw_progress, 2.8)
        set_tint("StoryBackground", progress, progress, progress, 1.0)
        if raw_progress >= 1.0 then
            prologue_state.mode = "intro_hold"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "intro_hold" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= prologue_state.intro_hold_seconds then
            prologue_state.mode = "line_reveal"
            prologue_state.elapsed = 0.0
            prologue_state.line_index = 0
            set_texture("StoryBackground", resolve_story_image("introduce", "Asset/Content/Texture/Story/introduce.png"))
            show_prologue_lines()
        end
        return true
    end

    if prologue_state.mode == "line_reveal" then
        return true
    end

    if prologue_state.mode == "line_complete_hold" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= 0.8 then
            prologue_state.mode = "outro_fade_out"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "outro_fade_out" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local progress = math.min(prologue_state.elapsed / prologue_state.outro_fade_seconds, 1.0)
        local alpha = 1.0 - progress
        local brightness = PROLOGUE_DIM_BACKGROUND + ((1.0 - PROLOGUE_DIM_BACKGROUND) * progress)
        set_tint("StoryBackground", brightness, brightness, brightness, 1.0)
        set_dialogue_alpha(alpha)
        if progress >= 1.0 then
            prologue_state.mode = "control_room_fade_in"
            prologue_state.elapsed = 0.0
            hide_dialogue_ui()
            set_texture("StoryBackground", resolve_story_image("control_room_off", "Asset/Content/Texture/Story/control_room_screen_off.png"))
            set_tint("StoryBackground", 1.0, 1.0, 1.0, 0.0)
        end
        return true
    end

    if prologue_state.mode == "control_room_fade_in" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local progress = math.min(prologue_state.elapsed / prologue_state.control_room_fade_seconds, 1.0)
        set_tint("StoryBackground", 1.0, 1.0, 1.0, progress)
        if progress >= 1.0 then
            play_story_sfx(resolve_story_sound("crt_on", DEFAULT_CRT_ON_SOUND_PATH))
            prologue_state.mode = "crt_audio_lead"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "crt_audio_lead" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= prologue_state.crt_lead_seconds then
            prologue_state.mode = "crt_power_on"
            prologue_state.elapsed = 0.0
            set_texture("StoryBackground", resolve_story_image("control_room_on", "Asset/Content/Texture/Story/control_room_screen_on.png"))
            set_tint("StoryBackground", 0.2, 0.2, 0.2, 1.0)
        end
        return true
    end

    if prologue_state.mode == "crt_power_on" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        local crt_progress = 1.0
        if prologue_state.crt_switch_delay_seconds > 0.0 then
            crt_progress = math.min(prologue_state.elapsed / prologue_state.crt_switch_delay_seconds, 1.0)
        end

        local brightness = 1.0
        if crt_progress < 0.12 then
            brightness = 0.0
        elseif crt_progress < 0.24 then
            brightness = 1.35
        elseif crt_progress < 0.38 then
            brightness = 0.18
        elseif crt_progress < 0.62 then
            brightness = 1.08
        elseif crt_progress < 0.82 then
            brightness = 0.82
        else
            brightness = 1.0
        end
        set_tint("StoryBackground", brightness, brightness, brightness, 1.0)

        if crt_progress >= 1.0 then
            set_tint("StoryBackground", 1.0, 1.0, 1.0, 1.0)
            prologue_state.mode = "post_crt_hold"
            prologue_state.elapsed = 0.0
        end
        return true
    end

    if prologue_state.mode == "post_crt_hold" then
        prologue_state.elapsed = prologue_state.elapsed + delta
        if prologue_state.elapsed >= prologue_state.post_crt_hold_seconds then
            start_story_sequence()
        end
        return true
    end

    return prologue_state.mode ~= "story"
end

apply_page = function(page)
    if type(page) ~= "table" then
        return
    end

    local background = resolve_page_background(page)
    local auto_page = is_auto_page(page)
    if background then
        if page.type == "dialogue" then
            set_background_immediately(page.background, background)
        else
            set_non_dialogue_background(background)
        end
    end

    if auto_page then
        reset_typewriter()
        clear_pending_dialogue_ui()
        auto_advance_timer = page.duration
        set_text("SpeakerName", "")
        set_text("DialogueText", "")
        set_text("NarrationText", "")
        set_text("PageHint", HOLD_SKIP_HINT_TEXT)
        set_visible("SpeakerName", false)
        set_visible("DialogueText", false)
        set_visible("NarrationText", false)
        set_visible("PageHint", true)
        return
    end

    auto_advance_timer = nil

    clear_pending_dialogue_ui()
    show_dialogue_ui(page)
end

local function advance_page()
    if #pages == 0 then
        return
    end

    if current_index >= #pages then
        play_story_sfx(resolve_story_sound("go", DEFAULT_GO_SOUND_PATH))
        hide_dialogue_ui()
        start_scene_change_flash()
        return
    end

    play_story_sfx(NEXT_PAGE_SOUND_PATH)

    local next_index = current_index + 1
    current_index = next_index
    apply_page(pages[current_index])
end

local function try_advance_page()
    if prologue_state.mode and prologue_state.mode ~= "story" then
        return try_advance_prologue()
    end

    if scene_change_timer then
        return false
    end

    if input_cooldown > 0.0 then
        return false
    end

    if typing_state.active then
        input_cooldown = INPUT_COOLDOWN_SECONDS
        complete_typewriter()
        return true
    end

    input_cooldown = INPUT_COOLDOWN_SECONDS
    advance_page()
    return true
end

local function load_story()
    scenario = ScenarioLoader.load(SCENARIO_PATH, load_json_file)
    if not scenario then
        warn("Failed to load story scenario:", SCENARIO_PATH)
        set_text("SpeakerName", "SYSTEM")
        set_text("DialogueText", "Scenario load failed.")
        set_text("PageHint", SCENARIO_PATH)
        return
    end

    pages = scenario.sequence or {}
    current_index = 1

    if #pages == 0 then
        set_text("SpeakerName", "SYSTEM")
        set_text("DialogueText", "No pages in scenario.")
        set_text("PageHint", SCENARIO_PATH)
        return
    end

    start_story_prologue()
end

function BeginPlay()
    cache_component("StoryBackground")
    cache_component("StoryBackgroundBaek0")
    cache_component("StoryBackgroundBaek1")
    cache_component("StoryBackgroundLim0")
    cache_component("StoryBackgroundLim1")
    cache_component("StoryBackgroundPod")
    cache_component("StoryBackgroundBaekLim0")
    cache_component("StoryBackgroundBaekLim1")
    cache_component("SpeakerName")
    cache_component("DialogueText")
    cache_component("NarrationText")
    cache_component("PageHint")
    cache_component("WhiteFlash")
    cache_component("NextPageButton")

    preload_story_background_layers()
    set_visible("StoryBackground", true)
    hide_dialogue_background_layers()
    hide_dialogue_ui()
    set_text("PageHint", HOLD_SKIP_HINT_TEXT)
    set_visible("PageHint", true)
    set_visible("WhiteFlash", false)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 0.0)

    load_story()
end

function Tick(dt)
    if story_finished then
        return
    end

    if update_skip_hold(dt) then
        return
    end

    if update_prologue(dt) then
        if input_cooldown > 0.0 then
            input_cooldown = math.max(0.0, input_cooldown - (dt or 0.0))
        end

        if prologue_state.mode == "line_reveal" then
            if GetKeyDown("SPACE") then
                try_advance_page()
                return
            end

            if ui["NextPageButton"] and ui["NextPageButton"]:WasClicked() then
                try_advance_page()
            end
        end
        return
    end

    if typing_state.active then
        typing_state.elapsed = typing_state.elapsed + (dt or 0.0)

        while typing_state.active
            and typing_state.visible_count < #typing_state.chars
            and typing_state.elapsed >= typing_state.interval do
            typing_state.elapsed = typing_state.elapsed - typing_state.interval
            typing_state.visible_count = typing_state.visible_count + 1

            local revealed_char = typing_state.chars[typing_state.visible_count]
            if (typing_state.visible_count % 3) == 0 and is_typing_sound_character(revealed_char) then
                play_story_sfx(TYPEWRITER_SOUND_PATH)
            end
        end

        update_typewriter_text()

        if typing_state.visible_count >= #typing_state.chars then
            typing_state.active = false
        end
    end

    if auto_advance_timer then
        auto_advance_timer = auto_advance_timer - (dt or 0.0)
        if auto_advance_timer <= 0.0 then
            auto_advance_timer = nil
            advance_page()
            return
        end
    end

    if scene_change_timer then
        scene_change_timer = scene_change_timer - (dt or 0.0)
        local progress = 1.0
        if scene_change_duration > 0.0 then
            progress = 1.0 - math.max(scene_change_timer, 0.0) / scene_change_duration
        end
        local alpha = ease_in_power(progress, 1.8)
        set_visible("WhiteFlash", true)
        set_tint("WhiteFlash", 1.0, 1.0, 1.0, alpha)
        if scene_change_timer <= 0.0 then
            scene_change_timer = nil
            finish_story("PlayerDev.Scene")
            return
        end
    end

    if input_cooldown > 0.0 then
        input_cooldown = math.max(0.0, input_cooldown - (dt or 0.0))
    end

    if GetKeyDown("SPACE") then
        try_advance_page()
        return
    end

    if ui["NextPageButton"] and ui["NextPageButton"]:WasClicked() then
        try_advance_page()
    end
end
