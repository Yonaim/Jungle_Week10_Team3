local Config = require("Game.Config")
local GameManager = require("Game.GameManager")
local AudioManager = require("Game.AudioManager")

local PlayerStatus = {
    max_hp = Config.player.max_hp,
    hp = Config.player.max_hp,
    invincible_time = Config.player.invincible_time,
    invincible_timer = 0.0,
    is_dead = false,
    invincible_end_logged = true,
}

local function log(message)
    if Config.debug.enable_log then
        print(message)
    end
end

local function safe_amount(amount, fallback)
    local value = amount or fallback
    if value < 0 then
        return 0
    end
    return value
end

function PlayerStatus.ResetForStart()
    PlayerStatus.max_hp = Config.player.max_hp
    PlayerStatus.hp = PlayerStatus.max_hp
    PlayerStatus.invincible_time = Config.player.invincible_time
    PlayerStatus.invincible_timer = 0.0
    PlayerStatus.is_dead = false
    PlayerStatus.invincible_end_logged = true

    log(
        "[PlayerStatus] ResetForStart hp=" .. tostring(PlayerStatus.hp) ..
        "/" .. tostring(PlayerStatus.max_hp) ..
        " invincible_time=" .. tostring(PlayerStatus.invincible_time)
    )
end

function PlayerStatus.Tick(dt)
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

function PlayerStatus.TakeDamage(amount)
    local damage = safe_amount(amount, Config.obstacle.default_damage)

    if PlayerStatus.is_dead then
        log("[PlayerStatus] TakeDamage ignored: already dead damage=" .. tostring(damage))
        return false
    end

    if GameManager.IsGameOver() then
        log("[PlayerStatus] TakeDamage ignored: GameOver damage=" .. tostring(damage))
        return false
    end

    if PlayerStatus.invincible_timer > 0.0 then
        log(
            "[PlayerStatus] Damage ignored due to invincible damage=" .. tostring(damage) ..
            " remaining=" .. tostring(PlayerStatus.invincible_timer)
        )
        return false
    end

    PlayerStatus.hp = PlayerStatus.hp - damage
    PlayerStatus.invincible_timer = PlayerStatus.invincible_time
    PlayerStatus.invincible_end_logged = false
    AudioManager.PlayHit()

    log(
        "[PlayerStatus] Damage applied damage=" .. tostring(damage) ..
        " hp=" .. tostring(PlayerStatus.hp) ..
        "/" .. tostring(PlayerStatus.max_hp)
    )

    if PlayerStatus.hp <= 0 then
        PlayerStatus.hp = 0
        PlayerStatus.is_dead = true
        log("[PlayerStatus] Dead by damage")
        GameManager.GameOver("HPZero")
    end

    return true
end

function PlayerStatus.Kill(reason)
    if PlayerStatus.is_dead then
        log("[PlayerStatus] Kill ignored: already dead reason=" .. tostring(reason))
        return false
    end

    PlayerStatus.hp = 0
    PlayerStatus.is_dead = true

    log("[PlayerStatus] Kill reason=" .. tostring(reason or "Unknown"))
    GameManager.GameOver(reason or "Kill")
    return true
end

function PlayerStatus.Heal(amount)
    if PlayerStatus.is_dead then
        log("[PlayerStatus] Heal ignored: dead")
        return false
    end

    local heal_amount = safe_amount(amount, 0)
    local old_hp = PlayerStatus.hp
    PlayerStatus.hp = math.min(PlayerStatus.max_hp, PlayerStatus.hp + heal_amount)

    log(
        "[PlayerStatus] Heal amount=" .. tostring(heal_amount) ..
        " hp=" .. tostring(old_hp) ..
        "->" .. tostring(PlayerStatus.hp) ..
        "/" .. tostring(PlayerStatus.max_hp)
    )
    return PlayerStatus.hp > old_hp
end

function PlayerStatus.IsDead()
    return PlayerStatus.is_dead
end

function PlayerStatus.IsInvincible()
    return PlayerStatus.invincible_timer > 0.0
end

function PlayerStatus.GetHP()
    return PlayerStatus.hp
end

function PlayerStatus.GetMaxHP()
    return PlayerStatus.max_hp
end

return PlayerStatus
