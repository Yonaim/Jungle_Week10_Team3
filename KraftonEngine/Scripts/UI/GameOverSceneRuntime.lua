local Config = require("Game.Config")
local GameManager = require("Game.GameManager")
local Scoreboard = require("Game.Scoreboard")

local result_data = nil
local title_text = nil
local body_text = nil
local status_text = nil
local save_button = nil
local score_saved = false
local title_loading = false
local waiting_title_confirm = false

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

local function build_fallback_result_data()
    return {
        score = GameManager.GetScore and GameManager.GetScore() or 0,
        logs = GameManager.GetLogs and GameManager.GetLogs() or 0,
        hotfix_count = GameManager.GetHotfixCount and GameManager.GetHotfixCount() or 0,
        critical_analysis_count = GameManager.GetCriticalAnalysisCount and GameManager.GetCriticalAnalysisCount() or 0,
        distance = GameManager.GetDistance and GameManager.GetDistance() or 0,
        coach_rank = GameManager.GetCoachRank and GameManager.GetCoachRank() or "C",
    }
end

local function build_result_body(data)
    local screen_config = Config.result_screen or {}
    local score = format_number(data.score or 0)
    local logs = format_number(data.logs or 0)
    local hotfix_count = tostring(math.floor(data.hotfix_count or 0)) .. "회"
    local crash_dump_analysis_count = tostring(math.floor(data.critical_analysis_count or 0)) .. "회"
    local depth = format_number(data.distance or 0) .. "m"
    local coach_rank = tostring(data.coach_rank or "C")

    return table.concat({
        "",
        "훈련 점수        " .. score,
        "수집 로그        " .. logs,
        "Hotfix 성공      " .. hotfix_count,
        "Crash Dump 분석  " .. crash_dump_analysis_count,
        "최대 진입 깊이    " .. depth,
        "",
        "코치 인정도      " .. coach_rank,
        "",
        tostring(screen_config.coach_name_baek or "백승현 코치") .. ":",
        "“" .. tostring(screen_config.coach_comment_baek or "") .. "”",
        "",
        tostring(screen_config.coach_name_lim or "임창근 코치") .. ":",
        "“" .. tostring(screen_config.coach_comment_lim or "") .. "”",
    }, "\n")
end

function BeginPlay()
    play_sfx("Sound.SFX.canceled.sound.effect", false)

    title_text = get_component("ResultTitle", "UUIScreenTextComponent_0")
    body_text = get_component("ResultBody", "UUIScreenTextComponent_1")
    status_text = get_component("SaveStatusText", "UUIScreenTextComponent_2")
    save_button = get_component("SaveScoreButton", "UIButtonComponent_0")

    if not title_text then
        warn("GameOverScene missing ResultTitle component")
    end
    if not body_text then
        warn("GameOverScene missing ResultBody component")
    end

    result_data = GameManager.GetResultData() or build_fallback_result_data()
    set_text(title_text, (Config.result_screen and Config.result_screen.title) or "DEBUG SESSION RESULT")
    set_text(body_text, build_result_body(result_data))
    set_text(status_text, "")
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
            open_message_popup("타이틀로 돌아갑니다")
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
