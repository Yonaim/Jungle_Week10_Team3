local Config = require("Game.Config")
local AudioManager = require("Game.AudioManager")

local GameManager = {
    State = {
        Ready = "Ready",
        Running = "Running",
        Playing = "Running",
        GameOver = "GameOver",
    },

    state = "Ready",
    score = 0,
    distance = 0.0,
    elapsed_time = 0.0,
    score_log_timer = 0.0,
}

local function log(message)
    if Config.debug.enable_log then
        print(message)
    end
end

local function score_log_interval()
    return Config.debug.score_log_interval or 1.0
end

function GameManager.StartGame()
    GameManager.state = GameManager.State.Running
    GameManager.score = 0
    GameManager.distance = 0.0
    GameManager.elapsed_time = 0.0
    GameManager.score_log_timer = 0.0

    log("[GameManager] StartGame state=Running score=0 distance=0 elapsed_time=0")
    AudioManager.PlayBGM()
end

function GameManager.Tick(dt, moved_distance)
    if GameManager.state ~= GameManager.State.Running then
        return
    end

    local safe_dt = dt or 0.0
    local safe_moved_distance = moved_distance or 0.0

    GameManager.distance = GameManager.distance + safe_moved_distance
    GameManager.elapsed_time = GameManager.elapsed_time + safe_dt
    GameManager.score = math.floor(
        GameManager.distance * Config.score.distance_weight
        + GameManager.elapsed_time * Config.score.survival_time_weight
    )

    GameManager.score_log_timer = GameManager.score_log_timer + safe_dt
    if GameManager.score_log_timer >= score_log_interval() then
        GameManager.score_log_timer = 0.0
        log(
            "[GameManager] ScoreLog score=" .. tostring(GameManager.score) ..
            " distance=" .. tostring(GameManager.distance) ..
            " elapsed_time=" .. tostring(GameManager.elapsed_time)
        )
    end
end

function GameManager.GameOver(reason)
    if GameManager.state == GameManager.State.GameOver then
        log("[GameManager] GameOver ignored: already GameOver")
        return
    end

    GameManager.state = GameManager.State.GameOver

    log("[GameManager] GameOver reason=" .. tostring(reason or "Unknown"))
    AudioManager.PlayGameOver()

    local should_stop_bgm = Config.audio.stop_bgm_on_game_over
    if should_stop_bgm == nil then
        should_stop_bgm = true
    end
    if should_stop_bgm then
        AudioManager.StopBGM()
    end

    log(
        "[GameManager] FinalScore score=" .. tostring(GameManager.score) ..
        " distance=" .. tostring(GameManager.distance) ..
        " elapsed_time=" .. tostring(GameManager.elapsed_time)
    )
    log("[GameManager] TODO ChangeLevel hook: GameOver")
end

function GameManager.IsRunning()
    return GameManager.state == GameManager.State.Running
end

function GameManager.IsGameOver()
    return GameManager.state == GameManager.State.GameOver
end

function GameManager.GetScore()
    return GameManager.score
end

function GameManager.GetDistance()
    return GameManager.distance
end

function GameManager.GetElapsedTime()
    return GameManager.elapsed_time
end

function GameManager.ChangeLevel(level_name)
    -- TODO: Connect real level transition after the scene flow exists.
    log("[GameManager] TODO ChangeLevel: " .. tostring(level_name))
end

-- Compatibility for older prototype scripts. New code should call StartGame().
GameManager.Start = GameManager.StartGame

return GameManager
