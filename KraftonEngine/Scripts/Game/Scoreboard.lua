local Scoreboard = {}

local ScoreboardSettings = {
    -- path: 스코어보드 저장 파일 경로입니다.
    path = "Saves/scoreboard.json",

    -- limit: 스코어보드에 유지할 최대 항목 수입니다.
    limit = 20,
}

local SCOREBOARD_PATH = ScoreboardSettings.path
local SCOREBOARD_LIMIT = ScoreboardSettings.limit

------------------------------------------------
-- Scoreboard 내부 헬퍼 함수들
------------------------------------------------

local function sanitize_nickname(nickname)
    local source = tostring(nickname or "")
    local result = {}

    for i = 1, #source do
        local character = string.sub(source, i, i)
        if string.match(character, "%a") then
            result[#result + 1] = string.upper(character)
            if #result >= 6 then
                break
            end
        end
    end

    return table.concat(result)
end

local function load_board()
    local data = load_json_file(SCOREBOARD_PATH)
    if type(data) ~= "table" then
        return {
            version = 1,
            entries = {},
        }
    end

    if type(data.entries) ~= "table" then
        if #data > 0 then
            data = {
                version = 1,
                entries = data,
            }
        else
            data.entries = {}
        end
    end

    if data.version == nil then
        data.version = 1
    end

    return data
end

local function trim_entries(entries)
    while #entries > SCOREBOARD_LIMIT do
        table.remove(entries)
    end
end

------------------------------------------------
-- Scoreboard 저장 / 조회 함수들
------------------------------------------------

function Scoreboard.SaveResult(nickname, result_data)
    local safe_nickname = sanitize_nickname(nickname)
    if safe_nickname == "" then
        return false, safe_nickname, SCOREBOARD_PATH
    end

    local board = load_board()
    local data = result_data or {}

    local entry = {
        nickname = safe_nickname,
        score = math.floor(data.score or 0),
        logs = math.floor(data.logs or 0),
        hotfix_count = math.floor(data.hotfix_count or 0),
        crash_dump_analysis_count = math.floor(data.critical_analysis_count or 0),
        max_depth_m = math.floor(data.distance or 0),
        coach_rank = tostring(data.coach_rank or "C"),
        reason = tostring(data.reason or "Unknown"),
    }

    table.insert(board.entries, entry)
    table.sort(board.entries, function(left, right)
        local left_score = tonumber(left.score) or 0
        local right_score = tonumber(right.score) or 0
        if left_score == right_score then
            return tostring(left.nickname or "") < tostring(right.nickname or "")
        end
        return left_score > right_score
    end)
    trim_entries(board.entries)

    return save_json_file(SCOREBOARD_PATH, board), safe_nickname, SCOREBOARD_PATH
end

function Scoreboard.Load()
    return load_board()
end

return Scoreboard
