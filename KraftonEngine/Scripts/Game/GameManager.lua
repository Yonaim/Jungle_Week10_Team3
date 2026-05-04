local Config = require("Game.Config")
local AudioManager = require("Game.AudioManager")

-- GameManager는 게임 전체 점수/진행도/HUD용 수치를 모아두는 전역 매니저입니다.
-- UI는 여기서 화면을 만들지 않고, 아래 getter만 읽어서 표시하면 됩니다.
local GameManager = {
    -- State는 게임 흐름 상태 문자열입니다. 실행 중 상태는 Running으로만 판정합니다.
    State = {
        Ready = "Ready",
        Running = "Running",
        GameOver = "GameOver",
    },

    -- state는 현재 게임 진행 상태입니다.
    state = "Ready",
    -- score는 HUD에 표시할 최종 점수입니다. 거리/시간 점수 + bonus_score가 합쳐진 값입니다.
    score = 0,
    -- bonus_score는 아이템/Hotfix/Critical Analysis 보너스가 Tick 재계산 때 사라지지 않게 따로 누적하는 값입니다.
    bonus_score = 0,
    -- distance는 실제 전진한 누적 거리입니다. HUD 작업자가 거리 표시할 때 읽으면 됩니다.
    distance = 0.0,
    -- elapsed_time은 달린 누적 시간입니다. 결과창/타이머 HUD에서 읽으면 됩니다.
    elapsed_time = 0.0,
    -- score_log_timer는 디버그 score log를 너무 자주 찍지 않게 조절하는 내부 타이머입니다.
    score_log_timer = 0.0,

    -- logs는 Log Fragment를 몇 개 먹었는지 세는 HUD용 수치입니다.
    logs = 0,
    -- trace는 Hotfix 게이지입니다. 100 이상이 되면 Hotfix가 발동되고 0으로 돌아갑니다.
    trace = 0,
    -- dumps는 Crash Dump 수집 수치입니다. 3개가 되면 Critical Analysis가 발동되고 0으로 돌아갑니다.
    dumps = 0,
    -- hotfix_count는 이번 런에서 Hotfix가 몇 번 발동됐는지 보여주는 수치입니다.
    hotfix_count = 0,
    -- critical_analysis_count는 Critical Analysis가 몇 번 발동됐는지 보여주는 수치입니다.
    critical_analysis_count = 0,

    -- stability/max_stability는 포드 안정도 HUD에 표시할 생존 수치입니다.
    -- 실제 감소/회복은 PlayerStatus가 담당하고, HUD가 읽기 쉽도록 여기에도 최신값을 동기화합니다.
    stability = Config.player.max_hp,
    max_stability = Config.player.max_hp,

    -- coach_approval은 코치 인정도 0~100 수치입니다.
    coach_approval = Config.coach.initial_approval,
    -- coach_rank는 coach_approval에서 계산되는 S/A/B/C/D/F 랭크입니다.
    coach_rank = "C",

    -- shield_count는 Crash Dump 분석 보상으로 쌓이는 1회 충돌 방어막 개수입니다.
    shield_count = 0,

    -- result_data는 GameOver 순간 결과창/다른 씬이 읽을 수 있게 보존하는 스냅샷입니다.
    result_data = nil,
}

-- log는 디버그 로그 출력 공통 함수입니다. Config.debug.enable_log로 전체 on/off 합니다.
local function log(message)
    if Config.debug.enable_log then
        print(message)
    end
end

-- clamp는 HUD용 수치가 범위를 넘지 않게 안전하게 잘라주는 작은 유틸입니다.
local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    end
    if value > max_value then
        return max_value
    end
    return value
end

-- score_log_interval은 설정이 비어 있어도 안전한 기본 로그 간격을 돌려줍니다.
local function score_log_interval()
    return Config.debug.score_log_interval or 1.0
end

-- calculate_coach_rank는 인정도 숫자를 기획 랭크 문자열로 바꿉니다.
-- HUD 작업자는 랭크 기준을 따로 복붙하지 말고 GetCoachRank()만 읽으면 됩니다.
local function calculate_coach_rank(approval)
    if approval >= 90 then
        return "S"
    end
    if approval >= 75 then
        return "A"
    end
    if approval >= 55 then
        return "B"
    end
    if approval >= 35 then
        return "C"
    end
    if approval >= 15 then
        return "D"
    end
    return "F"
end

-- refresh_coach_rank는 coach_approval이 바뀐 뒤 랭크를 최신 상태로 맞춥니다.
local function refresh_coach_rank()
    GameManager.coach_rank = calculate_coach_rank(GameManager.coach_approval)
end

-- build_result_data는 GameOver 순간 결과창에서 읽을 데이터를 한 번에 모읍니다.
-- 실제 UI/씬 전환은 아직 붙이지 않고, 다른 작업자가 이 테이블을 읽어 결과창을 만들면 됩니다.
local function build_result_data(reason)
    return {
        reason = reason or "Unknown",
        score = GameManager.score,
        distance = GameManager.distance,
        elapsed_time = GameManager.elapsed_time,
        logs = GameManager.logs,
        trace = GameManager.trace,
        dumps = GameManager.dumps,
        hotfix_count = GameManager.hotfix_count,
        critical_analysis_count = GameManager.critical_analysis_count,
        stability = GameManager.stability,
        max_stability = GameManager.max_stability,
        coach_approval = GameManager.coach_approval,
        coach_rank = GameManager.coach_rank,
        shield_count = GameManager.shield_count,
    }
end

function GameManager.StartGame()
    -- StartGame은 한 런의 모든 HUD용 수치를 초기화하는 지점입니다.
    -- 비주얼/HUD 담당자가 "게임 시작 시 초기값"을 확인해야 하면 여기 값을 보면 됩니다.
    -- 게임 시작 진입점은 StartGame 하나로 고정해 호출 흐름을 추적하기 쉽게 합니다.
    GameManager.state = GameManager.State.Running
    GameManager.score = 0
    GameManager.bonus_score = 0
    GameManager.distance = 0.0
    GameManager.elapsed_time = 0.0
    GameManager.score_log_timer = 0.0

    GameManager.logs = 0
    GameManager.trace = 0
    GameManager.dumps = 0
    GameManager.hotfix_count = 0
    GameManager.critical_analysis_count = 0
    GameManager.stability = Config.player.max_hp
    GameManager.max_stability = Config.player.max_hp
    GameManager.coach_approval = Config.coach.initial_approval or 50
    refresh_coach_rank()
    GameManager.shield_count = 0
    GameManager.result_data = nil

    log("[GameManager] StartGame state=Running score=0 distance=0 elapsed_time=0")
    AudioManager.PlayBGM()
end

function GameManager.Tick(dt, moved_distance)
    -- Tick은 거리/시간 기반 점수를 갱신합니다.
    -- 아이템 보너스는 bonus_score로 유지해서 다음 프레임 재계산 때도 사라지지 않습니다.
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
    ) + GameManager.bonus_score

    GameManager.score_log_timer = GameManager.score_log_timer + safe_dt
    if GameManager.score_log_timer >= score_log_interval() then
        GameManager.score_log_timer = 0.0
        log(
            "[GameManager] ScoreLog score=" .. tostring(GameManager.score) ..
            " distance=" .. tostring(GameManager.distance) ..
            " elapsed_time=" .. tostring(GameManager.elapsed_time) ..
            " logs=" .. tostring(GameManager.logs) ..
            " trace=" .. tostring(GameManager.trace) ..
            " dumps=" .. tostring(GameManager.dumps) ..
            " stability=" .. tostring(GameManager.stability) ..
            "/" .. tostring(GameManager.max_stability) ..
            " approval=" .. tostring(GameManager.coach_approval) ..
            " rank=" .. tostring(GameManager.coach_rank)
        )
    end
end

function GameManager.AddScore(amount, reason)
    -- 아이템/보너스 점수 공통 진입점입니다.
    -- score와 bonus_score를 같이 갱신해서 즉시 HUD/log에 보이고, 다음 Tick 재계산 후에도 유지되게 합니다.
    local safe_amount = amount or 0
    GameManager.bonus_score = GameManager.bonus_score + safe_amount
    GameManager.score = GameManager.score + safe_amount
    log(
        "[GameManager] AddScore amount=" .. tostring(safe_amount) ..
        " score=" .. tostring(GameManager.score) ..
        " reason=" .. tostring(reason or "Unknown")
    )
end

function GameManager.SetStabilitySnapshot(stability, max_stability)
    -- PlayerStatus 내부 저장값을 포드 안정도 HUD 스냅샷으로 반영합니다.
    -- PlayerStatus가 실제 값을 바꾼 직후 이 함수로 GameManager/HUD 수치를 동기화합니다.
    GameManager.max_stability = max_stability or GameManager.max_stability or Config.player.max_hp
    GameManager.stability = clamp(stability or GameManager.stability or GameManager.max_stability, 0, GameManager.max_stability)
end

function GameManager.AddCoachApproval(amount, reason)
    -- 코치 인정도 공통 변경 함수입니다.
    -- HUD 작업자는 이 값이 바뀔 때 coach_approval/coach_rank만 다시 읽으면 됩니다.
    local old_approval = GameManager.coach_approval
    GameManager.coach_approval = clamp(GameManager.coach_approval + (amount or 0), 0, 100)
    refresh_coach_rank()
    log(
        "[GameManager] CoachApproval " .. tostring(old_approval) ..
        "->" .. tostring(GameManager.coach_approval) ..
        " rank=" .. tostring(GameManager.coach_rank) ..
        " reason=" .. tostring(reason or "Unknown")
    )
end

function GameManager.OnLogCollected()
    -- Log Fragment를 먹었을 때 코치 인정도가 소폭 올라가는 이벤트입니다.
    GameManager.AddCoachApproval(Config.coach.log_collected_delta or 1, "LogCollected")
end

function GameManager.OnObstacleAvoided()
    -- TODO: 장애물을 실제로 지나쳤는지 판정하는 지점이 생기면 여기서 호출하면 됨.
    -- 지금 마감 전에는 장애물 회피 판정까지 새로 만들지 않고, 이벤트 함수만 먼저 준비합니다.
    GameManager.AddCoachApproval(Config.coach.obstacle_avoided_delta or 1, "ObstacleAvoided")
end

function GameManager.OnNearMiss()
    -- TODO: 장애물과 플레이어 사이의 최소 거리/시간을 계산할 수 있게 되면 여기서 호출하면 됨.
    -- 지금 마감 전에는 판정 로직까지 넣지 않음. 나중에 이 값으로 아슬아슬 회피 연출 붙이면 됨.
    GameManager.AddCoachApproval(Config.coach.near_miss_delta or 3, "NearMiss")
end

function GameManager.OnCommandSuccess()
    -- 추후 명령 입력/터미널 성공 같은 시스템이 생기면 여기서 인정도 상승을 연결하면 됩니다.
    GameManager.AddCoachApproval(Config.coach.command_success_delta or 2, "CommandSuccess")
end

function GameManager.OnCommandFailed()
    -- 추후 명령 실패 이벤트가 생기면 여기서 인정도 하락을 연결하면 됩니다.
    GameManager.AddCoachApproval(Config.coach.command_failed_delta or -3, "CommandFailed")
end

function GameManager.OnHotfixApplied()
    -- Hotfix 발동 성공 이벤트입니다. 여기서 화면에 "HOTFIX APPLIED" 띄우면 됨.
    GameManager.AddCoachApproval(Config.coach.hotfix_delta or 8, "HotfixApplied")
end

function GameManager.OnCrashDumpAnalyzed()
    -- Critical Analysis 발동 성공 이벤트입니다. 여기서 큰 분석 완료 이펙트 붙이면 됨.
    GameManager.AddCoachApproval(Config.coach.critical_analysis_delta or 10, "CrashDumpAnalyzed")
end

function GameManager.OnPlayerHit()
    -- 장애물 충돌 이벤트입니다. 여기서 화면 흔들림/위험 HUD 연출을 나중에 붙이면 됨.
    GameManager.AddCoachApproval(Config.coach.player_hit_delta or -5, "PlayerHit")
end

function GameManager.ApplyHotfix()
    -- trace 100%가 된 순간 들어오는 Hotfix 발동 지점입니다.
    -- 여기서 화면에 HOTFIX APPLIED 띄우면 됨. 실제 visual effect는 지금 만들지 않습니다.
    GameManager.trace = 0
    GameManager.hotfix_count = GameManager.hotfix_count + 1
    GameManager.AddScore(Config.collectible.hotfix_score or 1000, "Hotfix")
    GameManager.OnHotfixApplied()
    log("[GameManager] Hotfix applied count=" .. tostring(GameManager.hotfix_count))

    return {
        hotfix_applied = true,
        recover_stability = Config.collectible.hotfix_stability_recover or 15,
    }
end

function GameManager.ApplyCriticalAnalysis()
    -- Crash Dump 3개를 모은 순간 들어오는 Critical Analysis 발동 지점입니다.
    -- 여기서 화면에 CRITICAL ANALYSIS 같은 연출 붙이면 됨. 실제 visual effect는 지금 만들지 않습니다.
    GameManager.dumps = 0
    GameManager.critical_analysis_count = GameManager.critical_analysis_count + 1
    GameManager.shield_count = GameManager.shield_count + (Config.collectible.critical_analysis_shield_reward or 1)
    GameManager.AddScore(Config.collectible.critical_analysis_score or 3000, "CriticalAnalysis")
    GameManager.OnCrashDumpAnalyzed()
    log(
        "[GameManager] CriticalAnalysis count=" .. tostring(GameManager.critical_analysis_count) ..
        " shield_count=" .. tostring(GameManager.shield_count)
    )

    return {
        critical_analysis_applied = true,
        shield_added = Config.collectible.critical_analysis_shield_reward or 1,
    }
end

function GameManager.CollectLogFragment()
    -- Log Fragment 획득 규칙입니다.
    -- HUD는 logs/score/trace를 읽으면 되고, 이 함수가 Hotfix까지 자동으로 이어줍니다.
    GameManager.logs = GameManager.logs + 1
    GameManager.trace = GameManager.trace + (Config.collectible.log_fragment_trace or 2)
    GameManager.AddScore(Config.collectible.log_fragment_score or 10, "LogFragment")
    GameManager.OnLogCollected()

    log(
        "[GameManager] LogFragment logs=" .. tostring(GameManager.logs) ..
        " trace=" .. tostring(GameManager.trace)
    )

    if GameManager.trace >= (Config.collectible.trace_max or 100) then
        return GameManager.ApplyHotfix()
    end

    return {
        hotfix_applied = false,
        recover_stability = 0,
    }
end

function GameManager.CollectCrashDump()
    -- Crash Dump 획득 규칙입니다.
    -- dumps가 3개가 되는 순간 Critical Analysis가 발동되고 shield_count가 올라갑니다.
    GameManager.dumps = GameManager.dumps + 1
    log("[GameManager] CrashDump dumps=" .. tostring(GameManager.dumps))

    if GameManager.dumps >= (Config.collectible.crash_dump_required or 3) then
        return GameManager.ApplyCriticalAnalysis()
    end

    return {
        critical_analysis_applied = false,
        shield_added = 0,
    }
end

function GameManager.ConsumeShield()
    -- shield_count가 있으면 충돌 1회를 안정도 피해 없이 막습니다.
    -- HUD 작업자는 GetShieldCount()/HasShield()만 읽으면 남은 방어막 표시가 가능합니다.
    if GameManager.shield_count <= 0 then
        return false
    end

    GameManager.shield_count = GameManager.shield_count - 1
    log("[GameManager] Shield consumed remain=" .. tostring(GameManager.shield_count))
    return true
end

function GameManager.GameOver(reason)
    -- GameOver는 결과 데이터를 보존하는 지점입니다.
    if GameManager.state == GameManager.State.GameOver then
        log("[GameManager] GameOver ignored: already GameOver")
        return
    end

    GameManager.state = GameManager.State.GameOver
    GameManager.result_data = build_result_data(reason)

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
        " elapsed_time=" .. tostring(GameManager.elapsed_time) ..
        " logs=" .. tostring(GameManager.logs) ..
        " trace=" .. tostring(GameManager.trace) ..
        " dumps=" .. tostring(GameManager.dumps) ..
        " stability=" .. tostring(GameManager.stability) ..
        "/" .. tostring(GameManager.max_stability) ..
        " approval=" .. tostring(GameManager.coach_approval) ..
        " rank=" .. tostring(GameManager.coach_rank)
    )

    local result_scene = Config.result_screen and Config.result_screen.scene_path or "game/gameresult.scene"
    log("[GameManager] LoadResultScene scene=" .. tostring(result_scene))
    if type(load_scene) == "function" then
        load_scene(result_scene)
    else
        log("[GameManager] LoadResultScene skipped: load_scene is not bound")
    end
end

function GameManager.IsRunning()
    return GameManager.state == GameManager.State.Running
end

function GameManager.IsGameOver()
    return GameManager.state == GameManager.State.GameOver
end

function GameManager.GetScore()
    -- HUD 작업자가 점수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.score
end

function GameManager.GetDistance()
    -- HUD 작업자가 주행 거리 표시할 때 이 함수만 읽으면 됨.
    return GameManager.distance
end

function GameManager.GetElapsedTime()
    -- HUD 작업자가 경과 시간 표시할 때 이 함수만 읽으면 됨.
    return GameManager.elapsed_time
end

function GameManager.GetLogs()
    -- HUD 작업자가 Log Fragment 개수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.logs
end

function GameManager.GetTrace()
    -- HUD 작업자가 Hotfix 게이지 표시할 때 이 함수만 읽으면 됨.
    return GameManager.trace
end

function GameManager.GetDumps()
    -- HUD 작업자가 Crash Dump 개수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.dumps
end

function GameManager.GetHotfixCount()
    -- HUD/결과창 작업자가 Hotfix 발동 횟수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.hotfix_count
end

function GameManager.GetCriticalAnalysisCount()
    -- HUD/결과창 작업자가 Critical Analysis 발동 횟수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.critical_analysis_count
end

function GameManager.GetStability()
    -- HUD 작업자가 포드 안정도 현재값 표시할 때 이 함수만 읽으면 됨.
    return GameManager.stability
end

function GameManager.GetMaxStability()
    -- HUD 작업자가 포드 안정도 최대값 표시할 때 이 함수만 읽으면 됨.
    return GameManager.max_stability
end

function GameManager.GetStabilityPercent()
    -- HUD 작업자가 안정도 게이지 퍼센트 표시할 때 이 함수만 읽으면 됨.
    if GameManager.max_stability <= 0 then
        return 0
    end
    return GameManager.stability / GameManager.max_stability
end

function GameManager.GetCoachApproval()
    -- HUD 작업자가 코치 인정도 숫자 표시할 때 이 함수만 읽으면 됨.
    return GameManager.coach_approval
end

function GameManager.GetCoachRank()
    -- HUD 작업자가 S/A/B/C/D/F 랭크 표시할 때 이 함수만 읽으면 됨.
    return GameManager.coach_rank
end

function GameManager.GetShieldCount()
    -- HUD 작업자가 방어막 개수 표시할 때 이 함수만 읽으면 됨.
    return GameManager.shield_count
end

function GameManager.HasShield()
    -- HUD 작업자가 방어막 아이콘 on/off만 필요하면 이 함수만 읽으면 됨.
    return GameManager.shield_count > 0
end

function GameManager.GetResultData()
    -- 결과창/다른 씬이 GameOver 이후 최종 데이터를 읽는 함수입니다.
    -- UI는 만들지 않고, 데이터가 사라지지 않도록 여기만 준비해 둡니다.
    return GameManager.result_data
end

function GameManager.ChangeLevel(level_name)
    -- TODO: 실제 결과창/씬 전환 흐름이 정해지면 여기서 안전하게 연결하면 됨.
    log("[GameManager] TODO ChangeLevel: " .. tostring(level_name))
end

return GameManager
