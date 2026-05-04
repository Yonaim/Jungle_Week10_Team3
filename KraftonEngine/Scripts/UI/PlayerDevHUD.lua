local Config = require("Game.Config")
local GameManager = require("Game.GameManager")
local ScenarioLoader = require("UI.ScenarioLoader")
local DialogueUtils = require("UI.DialogueUtils")

print("[PlayerDevHUD] module loaded")

local DEFAULT_RANK_TEXTURE = "Asset/Content/Texture/UI/rank_c.png"
local DIALOGUE_PATH = "Asset/Content/Data/Dialogues/play.dialogue.json"
local DIALOGUE_SOUND_PATH = "Asset/Content/Sound/SFX/dialogue.mp3"
local EVENT_SOUND_BY_TOKEN = {
    agents_are_go = "Asset/Content/Sound/SFX/agents-are-go.mp3",
    pickup_log = "Asset/Content/Sound/SFX/story_next.mp3",
    pickup_dump = "Asset/Content/Sound/SFX/glitch_noise.wav",
    hit_error = "Asset/Content/Sound/SFX/arwing-hit-obstacle.mp3",
    alarm_warning = "Asset/Content/Sound/SFX/windows-98-error.mp3",
    hotfix_ready = "Asset/Content/Sound/SFX/go.mp3",
    hotfix_apply = "Asset/Content/Sound/SFX/crt-on.mp3",
    critical_analysis = "Asset/Content/Sound/SFX/glitch_noise.wav",
    speed_up = "Asset/Content/Sound/SFX/fired-sound-effect.mp3",
    near_miss = "Asset/Content/Sound/SFX/canceled-sound-effect.mp3",
    shield_block = "Asset/Content/Sound/SFX/story_next.mp3",
    game_over = "Asset/Content/Sound/SFX/windows-98-error.mp3",
}
local DEFAULT_EVENT_SOUND_BY_TRIGGER = {
    onRunStart = "Asset/Content/Sound/SFX/agents-are-go.mp3",
    onCollectLog = "Asset/Content/Sound/SFX/story_next.mp3",
    onCollectCrashDump = "Asset/Content/Sound/SFX/glitch_noise.wav",
    onHitObstacle = "Asset/Content/Sound/SFX/arwing-hit-obstacle.mp3",
    onShieldBlocked = "Asset/Content/Sound/SFX/story_next.mp3",
    onStabilityLow = "Asset/Content/Sound/SFX/windows-98-error.mp3",
    onHotfixReady = "Asset/Content/Sound/SFX/go.mp3",
    onApplyHotfix = "Asset/Content/Sound/SFX/crt-on.mp3",
    onCriticalAnalysis = "Asset/Content/Sound/SFX/glitch_noise.wav",
    onSpeedUp = "Asset/Content/Sound/SFX/fired-sound-effect.mp3",
    onNearMiss = "Asset/Content/Sound/SFX/canceled-sound-effect.mp3",
    onGameOver = "Asset/Content/Sound/SFX/windows-98-error.mp3",
}
local TYPEWRITER_INTERVAL_SECONDS = 0.028
local PORTRAIT_ANIMATION_INTERVAL_SECONDS = 0.12
local DIALOGUE_HOLD_SECONDS = 1.8
local DIALOGUE_MAX_LINE_WIDTH_UNITS = 16
local DIALOGUE_CONTINUATION_INDENT = ""
local WINDOW_TEXTURES = {
    BAEK_COMMANDER = {
        closed = "Asset/Content/Texture/UI/window_portrait_baek_0.png",
        open = "Asset/Content/Texture/UI/window_portrait_baek_1.png",
    },
    LIM_COMMANDER = {
        closed = "Asset/Content/Texture/UI/window_portrait_lim_0.png",
        open = "Asset/Content/Texture/UI/window_portrait_lim_1.png",
    },
}
local RANK_TEXTURE_BY_CODE = {
    s = "Asset/Content/Texture/UI/rank_s.png",
    a = "Asset/Content/Texture/UI/rank_a.png",
    b = "Asset/Content/Texture/UI/rank_b.png",
    c = "Asset/Content/Texture/UI/rank_c.png",
    d = "Asset/Content/Texture/UI/rank_c.png",
    f = "Asset/Content/Texture/UI/rank_f.png",
}

local title_text = nil
local core_metrics_text = nil
local run_details_text = nil
local approval_value_text = nil
local approval_gauge_fill = nil
local rank_marker = nil
local dialogue_window = nil
local dialogue_speaker_text = nil
local dialogue_message_text = nil

local last_snapshot_key = nil
local last_rank_texture = nil
local last_approval_ratio = nil
local dialogue_data = nil
local dialogue_entries = {}
local dialogue_entries_by_trigger = {}
local last_dialogue_id = nil
local current_dialogue_entry = nil
local dialogue_audio_handle = nil
local last_event_sound_path = nil
local dialogue_hold_timer = 0.0
local portrait_anim_elapsed = 0.0
local portrait_frame_open = false
local current_window_textures = WINDOW_TEXTURES.BAEK_COMMANDER
local typing_state = DialogueUtils.create_typewriter_state(TYPEWRITER_INTERVAL_SECONDS)

local function log(message)
    if Config.debug and Config.debug.enable_log then
        print("[PlayerDevHUD] " .. tostring(message))
    end
end

local function get_component(name)
    local component = obj:GetComponent(name)
    if component and component:IsValid() then
        return component
    end
    return nil
end

local function set_text(component, text)
    if component then
        component:SetText(text)
    end
end

local function set_texture(component, texture_path)
    if component and texture_path and texture_path ~= "" then
        component:SetTexture(texture_path)
    end
end

local function set_screen_position(component, x, y)
    if component then
        component:SetScreenPositionXYZ(x, y, 0.0)
    end
end

local function set_screen_size(component, width, height)
    if component then
        component:SetScreenSizeXYZ(width, height, 0.0)
    end
end

local function format_number(value)
    local formatted = tostring(math.floor(value or 0))
    local sign = ""
    if string.sub(formatted, 1, 1) == "-" then
        sign = "-"
        formatted = string.sub(formatted, 2)
    end

    local parts = {}
    while #formatted > 3 do
        table.insert(parts, 1, string.sub(formatted, -3))
        formatted = string.sub(formatted, 1, #formatted - 3)
    end
    table.insert(parts, 1, formatted)

    return sign .. table.concat(parts, ",")
end

local function format_decimal(value)
    return string.format("%.1f", value or 0)
end

local function format_percent(current, max_value)
    local safe_max = max_value or 0
    if safe_max <= 0 then
        return "0%"
    end

    local ratio = math.max(0, math.min((current or 0) / safe_max, 1))
    return tostring(math.floor(ratio * 100 + 0.5)) .. "%"
end

local function resolve_rank_texture(rank_code)
    local normalized = string.lower(tostring(rank_code or "c"))
    return RANK_TEXTURE_BY_CODE[normalized] or DEFAULT_RANK_TEXTURE
end

local function build_snapshot_key(data)
    return table.concat({
        tostring(data.reason or ""),
        tostring(data.score or 0),
        tostring(data.distance or 0),
        tostring(data.elapsed_time or 0),
        tostring(data.logs or 0),
        tostring(data.trace or 0),
        tostring(data.dumps or 0),
        tostring(data.hotfix_count or 0),
        tostring(data.critical_analysis_count or 0),
        tostring(data.stability or 0),
        tostring(data.max_stability or 0),
        tostring(data.coach_approval or 0),
        tostring(data.coach_rank or "C"),
        tostring(data.shield_count or 0),
    }, "|")
end

local function build_core_metrics_text(data)
    local trace_max = (Config.collectible and Config.collectible.trace_max) or 100
    local dumps_required = (Config.collectible and Config.collectible.crash_dump_required) or 3

    return table.concat({
        "SCORE           " .. format_number(data.score or 0),
        "LOGS            " .. format_number(data.logs or 0),
        "TRACE           " .. format_percent(data.trace or 0, trace_max),
        "DUMPS           " .. format_number(data.dumps or 0) .. "/" .. format_number(dumps_required),
        "STABILITY       " .. format_percent(data.stability or 0, data.max_stability or 0),
    }, "\n")
end

local function build_run_details_text(data)
    return table.concat({
        "DISTANCE        " .. format_decimal(data.distance or 0) .. "m",
        "TIME            " .. format_decimal(data.elapsed_time or 0) .. "s",
    }, "\n")
end

local APPROVAL_GAUGE = {
    x = 1496.0,
    y = 236.0,
    width = 316.0,
    height = 18.0,
}

local RANK_MARKER = {
    width = 110.0,
    height = 48.0,
    offset_y = -42.0,
}

local function refresh_approval_gauge(data)
    local approval = math.max(0, math.min(data.coach_approval or 0, 100))
    local ratio = approval / 100.0

    if ratio ~= last_approval_ratio then
        local fill_width = APPROVAL_GAUGE.width * ratio
        if fill_width < 1.0 then
            fill_width = 1.0
        end

        set_screen_size(approval_gauge_fill, fill_width, APPROVAL_GAUGE.height)

        local marker_x = APPROVAL_GAUGE.x + APPROVAL_GAUGE.width * ratio - (RANK_MARKER.width * 0.5)
        local min_marker_x = APPROVAL_GAUGE.x - (RANK_MARKER.width * 0.5)
        local max_marker_x = APPROVAL_GAUGE.x + APPROVAL_GAUGE.width - (RANK_MARKER.width * 0.5)
        marker_x = math.max(min_marker_x, math.min(marker_x, max_marker_x))
        set_screen_position(rank_marker, marker_x, APPROVAL_GAUGE.y + RANK_MARKER.offset_y)

        last_approval_ratio = ratio
    end

    set_text(approval_value_text, format_number(approval))
end

local function refresh_hud()
    local data = GameManager.GetResultData and GameManager.GetResultData() or nil
    if type(data) ~= "table" then
        return
    end

    local snapshot_key = build_snapshot_key(data)
    if snapshot_key ~= last_snapshot_key then
        set_text(title_text, "PLAYER DEV FUD  [" .. tostring(data.coach_rank or "C") .. "]")
        set_text(core_metrics_text, build_core_metrics_text(data))
        set_text(run_details_text, build_run_details_text(data))
        last_snapshot_key = snapshot_key
    end

    refresh_approval_gauge(data)

    local rank_texture = resolve_rank_texture(data.coach_rank)
    if rank_texture ~= last_rank_texture then
        set_texture(rank_marker, rank_texture)
        last_rank_texture = rank_texture
    end
end

local function stop_dialogue_audio()
    if dialogue_audio_handle and dialogue_audio_handle ~= "" then
        stop_audio_by_handle(dialogue_audio_handle)
    end
    dialogue_audio_handle = nil
end

local function resolve_event_sound_path(token)
    if type(token) ~= "string" or token == "" then
        return nil
    end

    if string.sub(token, 1, 6) == "Asset/" then
        return token
    end

    return EVENT_SOUND_BY_TOKEN[token]
end

local function play_dialogue_event_sfx(entry)
    local selected_path = nil
    local sounds = type(entry) == "table" and entry.sounds or nil
    if type(sounds) == "table" then
        for index = 1, #sounds do
            local token = sounds[index]
            if type(token) == "string" and string.sub(token, 1, 6) ~= "voice_" then
                local resolved = resolve_event_sound_path(token)
                if resolved and resolved ~= "" then
                    selected_path = resolved
                    break
                end
            end
        end
    end

    if not selected_path then
        selected_path = DEFAULT_EVENT_SOUND_BY_TRIGGER[type(entry) == "table" and entry.trigger or ""]
    end

    last_event_sound_path = selected_path
    if selected_path and selected_path ~= "" then
        play_sfx(selected_path, false)
    end
end

local function is_dialogue_audio_playing()
    return dialogue_audio_handle ~= nil
        and dialogue_audio_handle ~= ""
        and is_audio_playing_by_handle(dialogue_audio_handle)
end

local function resolve_window_textures(speaker_id)
    return WINDOW_TEXTURES[speaker_id] or WINDOW_TEXTURES.BAEK_COMMANDER
end

local function set_window_frame(opened)
    local frame_path = current_window_textures and (opened and current_window_textures.open or current_window_textures.closed) or nil
    set_texture(dialogue_window, frame_path)
end

local function update_dialogue_text()
    set_text(dialogue_message_text, DialogueUtils.get_visible_text(typing_state, ""))
end

local function load_dialogue_entries()
    dialogue_data = ScenarioLoader.load(DIALOGUE_PATH, load_json_file)
    if type(dialogue_data) ~= "table" or type(dialogue_data.dialogues) ~= "table" then
        dialogue_entries = {}
        log("Failed to load dialogue data from " .. tostring(DIALOGUE_PATH))
        return
    end

    dialogue_entries = {}
    dialogue_entries_by_trigger = {}
    for index = 1, #dialogue_data.dialogues do
        local entry = dialogue_data.dialogues[index]
        if type(entry) == "table" and type(entry.message) == "string" and entry.message ~= "" then
            dialogue_entries[#dialogue_entries + 1] = entry

            local trigger_name = type(entry.trigger) == "string" and entry.trigger or ""
            if trigger_name ~= "" then
                if type(dialogue_entries_by_trigger[trigger_name]) ~= "table" then
                    dialogue_entries_by_trigger[trigger_name] = {}
                end
                dialogue_entries_by_trigger[trigger_name][#dialogue_entries_by_trigger[trigger_name] + 1] = entry
            end
        end
    end

    log("Loaded dialogue entries count=" .. tostring(#dialogue_entries))
end

local function pick_random_dialogue_entry(entries)
    local pool = type(entries) == "table" and entries or dialogue_entries
    local count = #pool
    if count <= 0 then
        return nil
    end

    if count == 1 then
        return pool[1]
    end

    local chosen = nil
    local guard = 0
    repeat
        chosen = pool[math.random(1, count)]
        guard = guard + 1
    until not chosen or chosen.id ~= last_dialogue_id or guard >= 8

    return chosen
end

local function pick_trigger_dialogue_entry(trigger_name)
    if type(trigger_name) ~= "string" or trigger_name == "" then
        return nil
    end

    local entries = dialogue_entries_by_trigger[trigger_name]
    if type(entries) ~= "table" or #entries <= 0 then
        return nil
    end

    return pick_random_dialogue_entry(entries)
end

local function pick_initial_dialogue_entry()
    local start_entries = dialogue_entries_by_trigger.onRunStart
    if type(start_entries) == "table" and #start_entries > 0 then
        for index = 1, #start_entries do
            local entry = start_entries[index]
            if type(entry) == "table" and entry.speaker == "BAEK_COMMANDER" then
                return entry
            end
        end

        return start_entries[1]
    end

    return pick_random_dialogue_entry()
end

local function start_dialogue_preview(entry)
    if type(entry) ~= "table" then
        return
    end

    log(
        "StartDialogue id=" .. tostring(entry.id) ..
        " trigger=" .. tostring(entry.trigger) ..
        " speaker=" .. tostring(entry.speaker)
    )

    current_dialogue_entry = entry
    last_dialogue_id = entry.id
    current_window_textures = resolve_window_textures(entry.speaker)
    portrait_anim_elapsed = 0.0
    portrait_frame_open = false
    set_window_frame(false)

    set_text(dialogue_speaker_text, ScenarioLoader.resolve_speaker_name(dialogue_data, entry.speaker))

    local message = DialogueUtils.format_terminal_message(tostring(entry.message or ""), {
        max_line_width_units = DIALOGUE_MAX_LINE_WIDTH_UNITS,
        continuation_indent = DIALOGUE_CONTINUATION_INDENT,
    })
    DialogueUtils.start_typewriter(typing_state, message, TYPEWRITER_INTERVAL_SECONDS)
    update_dialogue_text()

    stop_dialogue_audio()
    play_dialogue_event_sfx(entry)
    dialogue_audio_handle = play_sfx(DIALOGUE_SOUND_PATH, false)
    dialogue_hold_timer = DIALOGUE_HOLD_SECONDS
end

local function advance_random_dialogue()
    local next_entry = pick_random_dialogue_entry()
    if not next_entry then
        set_text(dialogue_speaker_text, "DIALOGUE OFFLINE")
        set_text(dialogue_message_text, "play.dialogue.json load failed.")
        return
    end

    start_dialogue_preview(next_entry)
end

local function is_preview_mode()
    local is_running = GameManager.IsRunning and GameManager.IsRunning() or false
    local is_paused = GameManager.IsPaused and GameManager.IsPaused() or false
    local is_game_over = GameManager.IsGameOver and GameManager.IsGameOver() or false
    return not is_running and not is_paused and not is_game_over
end

local function consume_runtime_dialogue_trigger()
    if not (GameManager.ConsumeDialogueTrigger and not is_preview_mode()) then
        return nil
    end

    local trigger_name = GameManager.ConsumeDialogueTrigger()
    if trigger_name then
        log("ConsumeTrigger trigger=" .. tostring(trigger_name))
    end
    return trigger_name
end

local function update_dialogue_preview(dt)
    if #dialogue_entries <= 0 then
        return
    end

    -- Runtime triggers should preempt the current preview so hit/low-stability
    -- events feel reactive instead of waiting for the previous line to finish.
    local runtime_trigger = consume_runtime_dialogue_trigger()
    if runtime_trigger then
        local runtime_entry = pick_trigger_dialogue_entry(runtime_trigger)
        if runtime_entry then
            start_dialogue_preview(runtime_entry)
            return
        end

        log("No dialogue entry found for trigger=" .. tostring(runtime_trigger))
    end

    local advanced = DialogueUtils.update_typewriter(typing_state, dt, nil)
    if advanced then
        update_dialogue_text()
    end

    if not typing_state.active and typing_state.visible_count >= #typing_state.chars then
        update_dialogue_text()
    end

    if is_dialogue_audio_playing() then
        portrait_anim_elapsed = portrait_anim_elapsed + (dt or 0.0)
        while portrait_anim_elapsed >= PORTRAIT_ANIMATION_INTERVAL_SECONDS do
            portrait_anim_elapsed = portrait_anim_elapsed - PORTRAIT_ANIMATION_INTERVAL_SECONDS
            portrait_frame_open = not portrait_frame_open
            set_window_frame(portrait_frame_open)
        end
    else
        portrait_anim_elapsed = 0.0
        if portrait_frame_open then
            portrait_frame_open = false
            set_window_frame(false)
        end
    end

    if typing_state.active or is_dialogue_audio_playing() then
        return
    end

    dialogue_hold_timer = dialogue_hold_timer - (dt or 0.0)
    if dialogue_hold_timer <= 0.0 and is_preview_mode() then
        advance_random_dialogue()
    end
end

function BeginPlay()
    print("[PlayerDevHUD] BeginPlay entered")
    math.randomseed(math.floor(time() * 1000) % 2147483647)

    title_text = get_component("FUDTitle")
    core_metrics_text = get_component("FUDCoreMetricsText")
    run_details_text = get_component("FUDRunDetailsText")
    approval_value_text = get_component("FUDApprovalValue")
    approval_gauge_fill = get_component("FUDApprovalGaugeFill")
    rank_marker = get_component("FUDRankMarker")
    dialogue_window = get_component("DialogueWindowPortrait")
    dialogue_speaker_text = get_component("DialogueSpeakerName")
    dialogue_message_text = get_component("DialogueMessage")

    last_snapshot_key = nil
    last_rank_texture = nil
    last_approval_ratio = nil
    current_window_textures = WINDOW_TEXTURES.BAEK_COMMANDER
    DialogueUtils.reset_typewriter(typing_state, TYPEWRITER_INTERVAL_SECONDS)

    refresh_hud()
    load_dialogue_entries()
    start_dialogue_preview(pick_initial_dialogue_entry())
end

function Tick(dt)
    refresh_hud()
    update_dialogue_preview(dt)
end

function EndPlay()
    stop_dialogue_audio()
end
