local Config = require("Game.Config")

local AudioManager = {}

AudioManager.owner = nil
AudioManager.bgm_component = nil
AudioManager.sfx_component = nil
AudioManager.enabled = true
AudioManager.bgm_started = false

local function log(message)
    if Config.debug.enable_log then
        print(message)
    end
end

local function is_valid_component(component)
    return component and component.IsValid and component:IsValid()
end

local function find_component(owner, type_name)
    if not owner or not owner.IsValid or not owner:IsValid() then
        return nil
    end

    local component = owner:GetComponentByType(type_name)
    if is_valid_component(component) then
        return component
    end

    return nil
end

function AudioManager.Initialize(owner)
    AudioManager.owner = owner
    AudioManager.enabled = Config.audio.enabled ~= false
    AudioManager.bgm_started = false

    log("[AudioManager] Initialize enabled=" .. tostring(AudioManager.enabled))

    if not owner or not owner.IsValid or not owner:IsValid() then
        log("[AudioManager] Initialize warning: invalid owner")
        AudioManager.bgm_component = nil
        AudioManager.sfx_component = nil
        return false
    end

    AudioManager.bgm_component = find_component(owner, "UBackgroundSoundComponent")
    if is_valid_component(AudioManager.bgm_component) then
        log("[AudioManager] BGM component found type=" .. tostring(AudioManager.bgm_component:GetTypeName()))
    else
        log("[AudioManager] BGM component missing: UBackgroundSoundComponent")
    end

    AudioManager.sfx_component = find_component(owner, "USFXComponent")
    if is_valid_component(AudioManager.sfx_component) then
        log("[AudioManager] SFX component found type=" .. tostring(AudioManager.sfx_component:GetTypeName()))
    else
        log("[AudioManager] SFX component missing: USFXComponent")
    end

    return is_valid_component(AudioManager.bgm_component) or is_valid_component(AudioManager.sfx_component)
end

function AudioManager.PlayBGM()
    if not AudioManager.enabled then
        log("[AudioManager] PlayBGM skip: audio disabled")
        return false
    end

    if AudioManager.bgm_started then
        log("[AudioManager] PlayBGM skip: already started")
        return true
    end

    local path_or_name = Config.audio.play_bgm
    if path_or_name == nil or path_or_name == "" then
        log("[AudioManager] PlayBGM skip: Config.audio.play_bgm is empty")
        return false
    end

    if not is_valid_component(AudioManager.bgm_component) then
        log("[AudioManager] PlayBGM failed: BGM component missing")
        return false
    end

    AudioManager.bgm_component:SetAudioCategory("background")
    AudioManager.bgm_component:SetAudioLooping(Config.audio.bgm_loop == true)
    AudioManager.bgm_component:SetAudioPath(path_or_name)

    log("[AudioManager] PlayBGM attempt sound=" .. tostring(path_or_name))
    local ok = AudioManager.bgm_component:PlayAudio(path_or_name)
    AudioManager.bgm_started = ok == true
    log("[AudioManager] PlayBGM result=" .. tostring(ok))
    return ok == true
end

function AudioManager.StopBGM()
    if not is_valid_component(AudioManager.bgm_component) then
        log("[AudioManager] StopBGM skip: BGM component missing")
        AudioManager.bgm_started = false
        return false
    end

    local ok = AudioManager.bgm_component:StopAudio()
    AudioManager.bgm_started = false
    log("[AudioManager] StopBGM result=" .. tostring(ok))
    return ok == true
end

function AudioManager.PlaySFX(sound_key)
    if not AudioManager.enabled then
        log("[AudioManager] PlaySFX skip: audio disabled key=" .. tostring(sound_key))
        return false
    end

    local path_or_name = Config.audio[sound_key]
    if path_or_name == nil or path_or_name == "" then
        log("[AudioManager] PlaySFX skip: Config.audio." .. tostring(sound_key) .. " is empty")
        return false
    end

    if not is_valid_component(AudioManager.sfx_component) then
        log("[AudioManager] PlaySFX failed: SFX component missing key=" .. tostring(sound_key))
        return false
    end

    AudioManager.sfx_component:SetAudioCategory("sfx")
    AudioManager.sfx_component:SetAudioLooping(false)
    AudioManager.sfx_component:SetAudioPath(path_or_name)

    log("[AudioManager] PlaySFX attempt key=" .. tostring(sound_key) .. " sound=" .. tostring(path_or_name))
    local ok = AudioManager.sfx_component:PlayAudio(path_or_name)
    log("[AudioManager] PlaySFX result key=" .. tostring(sound_key) .. " ok=" .. tostring(ok))
    return ok == true
end

function AudioManager.PlayHit()
    return AudioManager.PlaySFX("hit_sfx")
end

function AudioManager.PlayJump()
    return AudioManager.PlaySFX("jump_sfx")
end

function AudioManager.PlaySlide()
    return AudioManager.PlaySFX("slide_sfx")
end

function AudioManager.PlayGameOver()
    return AudioManager.PlaySFX("game_over_sfx")
end

function AudioManager.SetEnabled(enabled)
    AudioManager.enabled = enabled ~= false
    log("[AudioManager] SetEnabled enabled=" .. tostring(AudioManager.enabled))
end

return AudioManager
