local story_scene_loading = false
local loading_overlay = nil
local loading_text = nil
local start_button = nil
local score_button = nil
local options_button = nil
local exit_button = nil
local logo_image = nil

local function get_component(name)
    local component = obj:GetComponent(name)
    if component and component:IsValid() then
        return component
    end

    warn("TitleScene 컴포넌트를 찾을 수 없습니다:", name)
    return nil
end

local function set_visible(component, visible)
    if component then
        component:SetVisible(visible)
    end
end

local function set_text(component, text)
    if component then
        component:SetText(text)
    end
end

local function set_tint(component, r, g, b, a)
    if component then
        component:SetTint(r, g, b, a)
    end
end

function BeginPlay()
    loading_overlay = get_component("LoadingOverlay")
    loading_text = get_component("LoadingText")
    start_button = get_component("UIButtonComponent_0")
    score_button = get_component("ScoreButton")
    options_button = get_component("OptionsButton")
    exit_button = get_component("ExitButton")
    logo_image = get_component("UUIImageComponent_0")

    set_visible(loading_overlay, false)
    set_visible(loading_text, false)
end

function ShowScoreboard()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return open_scoreboard_popup("Saves/scoreboard.json")
end

function ShowOptions()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return open_title_options_popup()
end

function ExitGame()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return request_exit_game()
end

function StartStoryScene()
    if story_scene_loading then
        return false
    end

    story_scene_loading = true
    stop_bgm()
    play_sfx("Sound.SFX.arwing.hit.obstacle", false)
    return load_scene("game/story.scene")
end

function Tick(dt)
    if story_scene_loading then
        return
    end

    if start_button and start_button:WasClicked() then
        StartStoryScene()
        return
    end

    if score_button and score_button:WasClicked() then
        ShowScoreboard()
        return
    end

    if options_button and options_button:WasClicked() then
        ShowOptions()
        return
    end

    if exit_button and exit_button:WasClicked() then
        ExitGame()
        return
    end
end
