local Config = require("Game.Config")
local GameManager = require("Game.GameManager")
local Scoreboard = require("Game.Scoreboard")

local DIALOGUE_PATH = "Asset/Content/Data/Dialogues/game_result.dialogue.json"
local DEFAULT_RANK_TEXTURE = "Asset/Content/Texture/UI/rank_c.png"
local RANK_TEXTURE_BY_CODE = {
    s = "Asset/Content/Texture/UI/rank_s.png",
    a = "Asset/Content/Texture/UI/rank_a.png",
    b = "Asset/Content/Texture/UI/rank_b.png",
    c = "Asset/Content/Texture/UI/rank_c.png",
    d = "Asset/Content/Texture/UI/rank_c.png",
    f = "Asset/Content/Texture/UI/rank_f.png",
}
local PREVIEW_RESULT_DATA = {
    reason = "EditorPreview",
    score = 128450,
    logs = 24,
    trace = 88,
    dumps = 2,
    hotfix_count = 3,
    critical_analysis_count = 1,
    distance = 1842.0,
    elapsed_time = 153.4,
    stability = 2,
    max_stability = 3,
    coach_approval = 84,
    coach_rank = "A",
    shield_count = 1,
}

local result_data = nil
local title_text = nil
local result_keys_text = nil
local result_values_text = nil
local coach_comments_text = nil
local status_text = nil
local save_button = nil
local rank_badge = nil
local score_saved = false
local title_loading = false
local waiting_title_confirm = false
local preview_mode = false

local function get_component(...)
    local names = { ... }
    for index = 1, #names do
        local name = names[index]
        local component = obj:GetComponent(name)
        if component and component:IsValid() then
            return component
        end
    end

    return nil
end

local function set_text(component, text)
    if component then
        component:SetText(text)
    end
end

local function set_label(component, text)
    if component then
        component:SetLabel(text)
    end
end

local function set_texture(component, texture_path)
    if component and texture_path and texture_path ~= "" then
        component:SetTexture(texture_path)
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

local function format_percent(current, max_value)
    local safe_max = max_value or 0
    if safe_max <= 0 then
        return "0%"
    end

    local ratio = math.max(0, math.min((current or 0) / safe_max, 1))
    return tostring(math.floor(ratio * 100 + 0.5)) .. "%"
end

local function copy_table(source)
    local copied = {}
    if type(source) ~= "table" then
        return copied
    end

    for key, value in pairs(source) do
        copied[key] = value
    end

    return copied
end

local function build_fallback_result_data()
    return {
        score = GameManager.GetScore and GameManager.GetScore() or 0,
        logs = GameManager.GetLogs and GameManager.GetLogs() or 0,
        trace = GameManager.GetTrace and GameManager.GetTrace() or 0,
        dumps = GameManager.GetDumps and GameManager.GetDumps() or 0,
        hotfix_count = GameManager.GetHotfixCount and GameManager.GetHotfixCount() or 0,
        critical_analysis_count = GameManager.GetCriticalAnalysisCount and GameManager.GetCriticalAnalysisCount() or 0,
        distance = GameManager.GetDistance and GameManager.GetDistance() or 0,
        stability = GameManager.GetStability and GameManager.GetStability() or 0,
        max_stability = GameManager.GetMaxStability and GameManager.GetMaxStability() or 0,
        coach_rank = GameManager.GetCoachRank and GameManager.GetCoachRank() or "C",
    }
end

local function resolve_result_data()
    if not (GameManager.IsGameOver and GameManager.IsGameOver()) then
        preview_mode = true
        return copy_table(PREVIEW_RESULT_DATA)
    end

    local data = GameManager.GetResultData and GameManager.GetResultData() or nil
    if type(data) == "table" then
        preview_mode = false
        return data
    end

    preview_mode = false
    return build_fallback_result_data()
end

local function build_metric_rows(data)
    local trace_max = (Config.collectible and Config.collectible.trace_max) or 100
    local dumps_required = (Config.collectible and Config.collectible.crash_dump_required) or 3

    return {
        { key = "SCORE", value = format_number(data.score or 0) },
        { key = "COLLECTED LOGS", value = format_number(data.logs or 0) },
        { key = "TRACE DATA", value = format_percent(data.trace or 0, trace_max) },
        { key = "CRASH DUMPS", value = format_number(data.dumps or 0) .. "/" .. format_number(dumps_required) },
        { key = "HOTFIX APPLIED", value = tostring(math.floor(data.hotfix_count or 0)) },
        { key = "CRITICAL ANALYSIS", value = tostring(math.floor(data.critical_analysis_count or 0)) },
        { key = "DISTANCE", value = format_number(data.distance or 0) .. "m" },
    }
end

local function normalize_rank(rank_code)
    local normalized = string.upper(tostring(rank_code or "C"))
    if normalized == "" then
        return "C"
    end

    return normalized
end

local function load_coach_dialogues(data)
    local screen_config = Config.result_screen or {}
    local result = {
        baek_name = screen_config.coach_name_baek or "백 사령관",
        lim_name = screen_config.coach_name_lim or "임 오퍼레이터",
        baek_message = screen_config.coach_comment_baek or "",
        lim_message = screen_config.coach_comment_lim or "",
    }

    local dialogue_root = load_json_file(DIALOGUE_PATH)
    if type(dialogue_root) ~= "table" or type(dialogue_root.dialogues) ~= "table" then
        return result
    end

    local rank = normalize_rank(data.coach_rank)
    local dialogues = dialogue_root.dialogues
    for index = 1, #dialogues do
        local entry = dialogues[index]
        if type(entry) == "table" and normalize_rank(entry.rank) == rank then
            if entry.speaker == "BAEK_COMMANDER" and entry.message and entry.message ~= "" then
                result.baek_message = tostring(entry.message)
            elseif entry.speaker == "LIM_COMMANDER" and entry.message and entry.message ~= "" then
                result.lim_message = tostring(entry.message)
            end
        end
    end

    return result
end

local function build_result_keys_text(data)
    local rows = build_metric_rows(data)
    local lines = {}
    for index = 1, #rows do
        lines[index] = rows[index].key
    end

    return table.concat(lines, "\n")
end

local function build_result_values_text(data)
    local rows = build_metric_rows(data)
    local lines = {}
    for index = 1, #rows do
        lines[index] = rows[index].value
    end

    return table.concat(lines, "\n")
end

local function build_coach_comments_text(data)
    local coach_dialogues = load_coach_dialogues(data)

    return table.concat({
        tostring(coach_dialogues.baek_name),
        "\"" .. tostring(coach_dialogues.baek_message or "") .. "\"",
        "",
        tostring(coach_dialogues.lim_name),
        "\"" .. tostring(coach_dialogues.lim_message or "") .. "\"",
    }, "\n")
end

local function resolve_rank_texture(rank_code)
    local normalized = string.lower(normalize_rank(rank_code))
    return RANK_TEXTURE_BY_CODE[normalized] or DEFAULT_RANK_TEXTURE
end

function BeginPlay()
    play_sfx("Sound.SFX.canceled.sound.effect", false)

    title_text = get_component("ResultTitle", "UUIScreenTextComponent_0")
    result_keys_text = get_component("ResultKeys", "UUIScreenTextComponent_1")
    result_values_text = get_component("ResultValues", "UUIScreenTextComponent_3")
    coach_comments_text = get_component("CoachComments", "UUIScreenTextComponent_4")
    rank_badge = get_component("RankBadge", "UUIImageComponent_1")
    status_text = get_component("SaveStatusText", "UUIScreenTextComponent_2")
    save_button = get_component("SaveScoreButton", "UIButtonComponent_0")

    if not title_text then
        warn("GameResultScene missing ResultTitle component")
    end
    if not result_keys_text then
        warn("GameResultScene missing ResultKeys component")
    end
    if not result_values_text then
        warn("GameResultScene missing ResultValues component")
    end
    if not coach_comments_text then
        warn("GameResultScene missing CoachComments component")
    end
    if not rank_badge then
        warn("GameResultScene missing RankBadge component")
    end

    result_data = resolve_result_data()
    set_text(title_text, (Config.result_screen and Config.result_screen.title) or "DEBUG SESSION RESULT")
    set_text(result_keys_text, build_result_keys_text(result_data))
    set_text(result_values_text, build_result_values_text(result_data))
    set_text(coach_comments_text, build_coach_comments_text(result_data))
    set_texture(rank_badge, resolve_rank_texture(result_data and result_data.coach_rank))
    set_text(status_text, preview_mode and "PREVIEW SAMPLE DATA" or "")
    set_label(save_button, "SAVE SCORE")
    score_saved = false
    title_loading = false
    waiting_title_confirm = false
end

function Tick(dt)
    local nickname = consume_score_save_popup_result()
    if nickname and not score_saved then
        local saved, safe_nickname, save_path = Scoreboard.SaveResult(nickname, result_data)
        if saved then
            score_saved = true
            waiting_title_confirm = true
            set_text(status_text, "SAVED: " .. tostring(safe_nickname))
            set_label(save_button, "SAVED")
            print("[Scoreboard] Saved to " .. tostring(save_path) .. " nickname=" .. tostring(safe_nickname) .. " score=" .. tostring(math.floor((result_data and result_data.score) or 0)))
            open_message_popup("Returning to title.")
        else
            set_text(status_text, "SAVE FAILED")
        end
    end

    if waiting_title_confirm and consume_message_popup_ok() then
        waiting_title_confirm = false
        GoToTitle()
    end
end

function SaveScore()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)

    if score_saved or waiting_title_confirm then
        return false
    end

    local score = math.floor((result_data and result_data.score) or 0)
    set_text(status_text, "ENTER NICKNAME")
    return open_score_save_popup(score)
end

function DelayedGoToTitle()
    wait(1.0)
    return load_scene("game/title.scene")
end

function GoToTitle()
    if title_loading then
        return false
    end

    title_loading = true
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return StartCoroutine("DelayedGoToTitle")
end
