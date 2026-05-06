local DebugConfig = require("Game.Config.Debug")
local PlayerStatusConfig = require("Game.Config.PlayerStatus")
local ObstacleConfig = require("Game.Config.Obstacle")
local GameManager = require("Game.GameManager")
local AudioManager = require("Game.AudioManager")
local Log = require("Common.Log")
local Math = require("Common.Math")

-- PlayerStatus는 플레이어 생존 상태를 관리합니다.
-- 외부 Lua 코드는 Stability API를 사용하고, 내부 hp/max_hp 값은 실제 저장 슬롯으로만 다룹니다.
local PlayerStatus = {
    -- max_hp는 내부 저장 이름이고, 공개 API에서는 포드 안정도 최대치(max_stability)로 다룹니다.
    max_hp = PlayerStatusConfig.max_hp,
    -- hp는 내부 저장 이름이고, 공개 API에서는 포드 안정도 현재값(stability)로 다룹니다.
    hp = PlayerStatusConfig.max_hp,
    -- invincible_time은 피격/방어막 발동 직후 중복 충돌을 막는 시간입니다.
    invincible_time = PlayerStatusConfig.invincible_time,
    -- invincible_timer는 현재 남은 무적 시간입니다.
    invincible_timer = 0.0,
    -- is_dead는 안정도가 0이 되었거나 낙사 등으로 사망했는지 나타냅니다.
    is_dead = false,
    -- invincible_end_logged는 무적 종료 로그를 한 번만 찍기 위한 내부 플래그입니다.
    invincible_end_logged = true,
}

local log = Log.MakeLogger(DebugConfig)

------------------------------------------------
-- 내부 동기화 함수들
------------------------------------------------

-- sync_stability_to_manager는 실제 안정도 값을 GameManager HUD 스냅샷에 맞춰 둡니다.
-- HUD 작업자는 PlayerStatus를 직접 몰라도 GameManager getter만 읽으면 안정도 표시가 가능합니다.
local function sync_stability_to_manager()
    GameManager.SetStabilitySnapshot(PlayerStatus.hp, PlayerStatus.max_hp)
end

-- start_invincible_window는 한 장애물에서 overlap/hit이 연달아 들어와도 여러 번 맞지 않게 막습니다.
local function start_invincible_window()
    PlayerStatus.invincible_timer = PlayerStatus.invincible_time
    PlayerStatus.invincible_end_logged = false
end

------------------------------------------------
-- 생명주기 함수들
------------------------------------------------

function PlayerStatus.ResetForStart()
    -- ResetForStart는 새 게임 시작 시 안정도/무적/사망 상태를 초기화합니다.
    -- 초기화 직후 GameManager에도 같은 안정도 값을 동기화합니다.
    PlayerStatus.max_hp = PlayerStatusConfig.max_hp
    PlayerStatus.hp = PlayerStatus.max_hp
    PlayerStatus.invincible_time = PlayerStatusConfig.invincible_time
    PlayerStatus.invincible_timer = 0.0
    PlayerStatus.is_dead = false
    PlayerStatus.invincible_end_logged = true
    sync_stability_to_manager()

    log(
        "[PlayerStatus] ResetForStart stability=" .. tostring(PlayerStatus.hp) ..
        "/" .. tostring(PlayerStatus.max_hp) ..
        " invincible_time=" .. tostring(PlayerStatus.invincible_time)
    )
end

function PlayerStatus.Tick(dt)
    -- Tick은 무적 시간을 줄이는 함수입니다. 안정도 자체는 여기서 자연 감소하지 않습니다.
    -- TODO: 오래 Log Fragment를 못 먹었을 때 인정도/안정도 하락이 필요하면 별도 타이머를 여기나 GameManager에 붙이면 됨.
    if PlayerStatus.invincible_timer <= 0.0 then
        return
    end

    PlayerStatus.invincible_timer = PlayerStatus.invincible_timer - (dt or 0.0)
    if PlayerStatus.invincible_timer <= 0.0 then
        PlayerStatus.invincible_timer = 0.0
        if not PlayerStatus.invincible_end_logged then
            log("[PlayerStatus] Invincible ended")
            PlayerStatus.invincible_end_logged = true
        end
    end
end

------------------------------------------------
-- 안정도 변경 함수들
------------------------------------------------

function PlayerStatus.DamageStability(amount)
    -- 포드 안정도 피해를 적용하는 단일 진입점입니다.
    -- 장애물 충돌 피해는 PlayerController에서 이 함수로 바로 연결합니다.
    local damage = Math.SafeNonNegative(amount, ObstacleConfig.default_damage)

    if PlayerStatus.is_dead then
        log("[PlayerStatus] DamageStability ignored: already dead damage=" .. tostring(damage))
        return false
    end

    if GameManager.IsGameOver() then
        log("[PlayerStatus] DamageStability ignored: GameOver damage=" .. tostring(damage))
        return false
    end

    if PlayerStatus.invincible_timer > 0.0 then
        log(
            "[PlayerStatus] Damage ignored due to invincible damage=" .. tostring(damage) ..
            " remaining=" .. tostring(PlayerStatus.invincible_timer)
        )
        return false
    end

    GameManager.OnPlayerHit()

    if GameManager.ConsumeShield() then
        -- shield가 있으면 충돌 1회를 안정도 피해 없이 막습니다.
        -- 여기서 방어막 깨지는 HUD/이펙트를 나중에 붙이면 됨. 실제 visual effect는 지금 만들지 않습니다.
        start_invincible_window()
        log("[PlayerStatus] Shield blocked damage=" .. tostring(damage))
        return true
    end

    PlayerStatus.hp = PlayerStatus.hp - damage
    start_invincible_window()
    AudioManager.PlayHit()
    sync_stability_to_manager()

    log(
        "[PlayerStatus] Stability damaged damage=" .. tostring(damage) ..
        " stability=" .. tostring(PlayerStatus.hp) ..
        "/" .. tostring(PlayerStatus.max_hp)
    )

    if PlayerStatus.hp <= 0 then
        PlayerStatus.hp = 0
        PlayerStatus.is_dead = true
        sync_stability_to_manager()
        log("[PlayerStatus] Stability empty")
        GameManager.GameOver("StabilityZero")
    end

    return true
end

function PlayerStatus.RecoverStability(amount)
    -- 포드 안정도 회복을 적용하는 단일 진입점입니다.
    -- Hotfix 성공 같은 회복 이벤트는 이 함수만 호출하면 HUD 값까지 같이 갱신됩니다.
    if PlayerStatus.is_dead then
        log("[PlayerStatus] RecoverStability ignored: dead")
        return false
    end

    local recover_amount = Math.SafeNonNegative(amount, 0)
    local old_hp = PlayerStatus.hp
    PlayerStatus.hp = math.min(PlayerStatus.max_hp, PlayerStatus.hp + recover_amount)
    sync_stability_to_manager()

    log(
        "[PlayerStatus] Stability recovered amount=" .. tostring(recover_amount) ..
        " stability=" .. tostring(old_hp) ..
        "->" .. tostring(PlayerStatus.hp) ..
        "/" .. tostring(PlayerStatus.max_hp)
    )
    return PlayerStatus.hp > old_hp
end

function PlayerStatus.Kill(reason)
    -- Kill은 낙사처럼 안정도 계산과 무관하게 즉시 GameOver가 필요할 때 쓰는 함수입니다.
    if PlayerStatus.is_dead then
        log("[PlayerStatus] Kill ignored: already dead reason=" .. tostring(reason))
        return false
    end

    PlayerStatus.hp = 0
    PlayerStatus.is_dead = true
    sync_stability_to_manager()

    log("[PlayerStatus] Kill reason=" .. tostring(reason or "Unknown"))
    GameManager.GameOver(reason or "Kill")
    return true
end

------------------------------------------------
-- 상태 조회 함수들
------------------------------------------------

function PlayerStatus.IsDead()
    -- 플레이어가 이미 죽었는지 확인하는 함수입니다.
    return PlayerStatus.is_dead
end

function PlayerStatus.IsInvincible()
    -- 현재 피격 직후 무적 상태인지 확인하는 함수입니다.
    return PlayerStatus.invincible_timer > 0.0
end

function PlayerStatus.GetStability()
    -- HUD 작업자가 포드 안정도 현재값을 PlayerStatus에서 직접 읽어야 하면 이 함수만 읽으면 됨.
    return PlayerStatus.hp
end

function PlayerStatus.GetMaxStability()
    -- HUD 작업자가 포드 안정도 최대값을 PlayerStatus에서 직접 읽어야 하면 이 함수만 읽으면 됨.
    return PlayerStatus.max_hp
end

function PlayerStatus.GetStabilityPercent()
    -- HUD 작업자가 안정도 게이지 비율을 PlayerStatus에서 직접 읽어야 하면 이 함수만 읽으면 됨.
    if PlayerStatus.max_hp <= 0 then
        return 0
    end
    return PlayerStatus.hp / PlayerStatus.max_hp
end

function PlayerStatus.IsStabilityEmpty()
    -- 안정도가 0인지 확인하는 함수입니다. GameOver 조건 연출을 붙일 때 읽으면 됩니다.
    return PlayerStatus.hp <= 0
end

return PlayerStatus
