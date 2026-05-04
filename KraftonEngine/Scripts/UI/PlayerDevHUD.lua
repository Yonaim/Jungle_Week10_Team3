local Config = require("Game.Config")
local GameManager = require("Game.GameManager")

local DEFAULT_RANK_TEXTURE = "Asset/Content/Texture/UI/rank_c.png"
local RANK_TEXTURE_BY_CODE = {
    s = "Asset/Content/Texture/UI/rank_s.png",
    a = "Asset/Content/Texture/UI/rank_a.png",
    b = "Asset/Content/Texture/UI/rank_b.png",
    c = "Asset/Content/Texture/UI/rank_c.png",
    d = "Asset/Content/Texture/UI/rank_c.png",
    f = "Asset/Content/Texture/UI/rank_f.png",
}

local title_text = nil
local body_text = nil
local rank_badge = nil
local last_snapshot_key = nil
local last_rank_texture = nil

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

local function build_body_text(data)
    local trace_max = (Config.collectible and Config.collectible.trace_max) or 100
    local dumps_required = (Config.collectible and Config.collectible.crash_dump_required) or 3
    local coach_name_baek = (Config.result_screen and Config.result_screen.coach_name_baek) or "백승현 사령관"
    local coach_name_lim = (Config.result_screen and Config.result_screen.coach_name_lim) or "임창근 사령관"

    return table.concat({
        "CORE METRICS",
        "SCORE           " .. format_number(data.score or 0),
        "LOGS            " .. format_number(data.logs or 0),
        "TRACE           " .. format_percent(data.trace or 0, trace_max),
        "DUMPS           " .. format_number(data.dumps or 0) .. "/" .. format_number(dumps_required),
        "STABILITY       " .. format_percent(data.stability or 0, data.max_stability or 0),
        "",
        "RUN DETAILS",
        "DISTANCE        " .. format_decimal(data.distance or 0) .. "m",
        "TIME            " .. format_decimal(data.elapsed_time or 0) .. "s",
        "HOTFIX APPLIED  " .. format_number(data.hotfix_count or 0),
        "CRITICAL        " .. format_number(data.critical_analysis_count or 0),
        "",
        "SUPPORT",
        "COACH RANK      " .. tostring(data.coach_rank or "C"),
        "APPROVAL        " .. format_number(data.coach_approval or 0),
        "SHIELD          " .. format_number(data.shield_count or 0),
        "STATE           " .. tostring(data.reason or "Unknown"),
        "",
        "COACH CHANNEL",
        tostring(coach_name_baek),
        "  runtime stable",
        tostring(coach_name_lim),
        "  keep pushing",
    }, "\n")
end

local function refresh_hud()
    local data = GameManager.GetResultData and GameManager.GetResultData() or nil
    if type(data) ~= "table" then
        return
    end

    local snapshot_key = build_snapshot_key(data)
    if snapshot_key ~= last_snapshot_key then
        set_text(title_text, "PLAYER DEV FUD  [" .. tostring(data.coach_rank or "C") .. "]")
        set_text(body_text, build_body_text(data))
        last_snapshot_key = snapshot_key
    end

    local rank_texture = resolve_rank_texture(data.coach_rank)
    if rank_texture ~= last_rank_texture then
        set_texture(rank_badge, rank_texture)
        last_rank_texture = rank_texture
    end
end

function BeginPlay()
    title_text = get_component("FUDTitle")
    body_text = get_component("FUDBody")
    rank_badge = get_component("FUDRankBadge")
    last_snapshot_key = nil
    last_rank_texture = nil
    refresh_hud()
end

function Tick(dt)
    refresh_hud()
end
