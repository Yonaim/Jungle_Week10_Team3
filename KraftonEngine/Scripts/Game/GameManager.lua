local GameManager = {
    State = {
        Ready = "Ready",
        Playing = "Playing",
        GameOver = "GameOver",
    },
    state = "Playing",
    score = 0.0,
    survival_time = 0.0,
    distance = 0.0,
    debug_score_log = false,
    score_log_interval = 1.0,
    score_log_timer = 0.0,
}

function GameManager.Start()
    GameManager.state = GameManager.State.Playing
    GameManager.score = 0.0
    GameManager.survival_time = 0.0
    GameManager.distance = 0.0
    GameManager.score_log_timer = 0.0
    print("[TempleRun] GameManager Start: Playing")
end

function GameManager.Tick(dt, distance_delta)
    if GameManager.state ~= GameManager.State.Playing then
        return
    end

    GameManager.survival_time = GameManager.survival_time + (dt or 0.0)
    GameManager.distance = GameManager.distance + (distance_delta or 0.0)
    GameManager.score = GameManager.distance

    if GameManager.debug_score_log then
        GameManager.score_log_timer = GameManager.score_log_timer + (dt or 0.0)
        if GameManager.score_log_timer >= GameManager.score_log_interval then
            GameManager.score_log_timer = 0.0
            print("[TempleRun] ScoreLog score=" .. tostring(math.floor(GameManager.score)) ..
                  " distance=" .. tostring(math.floor(GameManager.distance)) ..
                  " survival_time=" .. tostring(math.floor(GameManager.survival_time)))
        end
    end
end

function GameManager.GameOver(reason)
    if GameManager.state == GameManager.State.GameOver then
        return
    end

    GameManager.state = GameManager.State.GameOver
    print("[TempleRun] GameOver: " .. (reason or "Unknown"))
    print("[TempleRun] Score: " .. tostring(math.floor(GameManager.score)))
end

function GameManager.Restart()
    GameManager.state = GameManager.State.Playing
    GameManager.score = 0.0
    GameManager.survival_time = 0.0
    GameManager.distance = 0.0
    GameManager.score_log_timer = 0.0
    print("[TempleRun] Restart")
end

function GameManager.IsGameOver()
    return GameManager.state == GameManager.State.GameOver
end

return GameManager
