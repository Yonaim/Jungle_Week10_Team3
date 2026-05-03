local story_scene_loading = false
local loading_overlay = nil
local loading_text = nil
local start_button = nil
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
    logo_image = get_component("UUIImageComponent_0")

    set_visible(loading_overlay, false)
    set_visible(loading_text, false)
end

function DelayedStartStoryScene()
    local dots = {
        "LOADING.",
        "LOADING..",
        "LOADING...",
        "LOADING....",
    }

    set_visible(loading_overlay, true)
    set_visible(loading_text, true)
    set_visible(start_button, false)

    play_sfx("Sound.SFX.go", false)

    local elapsed = 0.0
    local step = 1
    while elapsed < 3.0 do
        local alpha = 0.20 + (elapsed / 3.0) * 0.70
        set_tint(loading_overlay, 0.0, 0.0, 0.0, alpha)
        set_tint(loading_text, 0.60, 0.88, 1.0, 0.75 + ((step % 2) * 0.25))
        set_text(loading_text, dots[step])

        if logo_image then
            logo_image:SetTint(1.0, 1.0, 1.0, 1.0 - elapsed / 3.0)
        end

        wait(0.3)
        elapsed = elapsed + 0.3
        step = step + 1
        if step > #dots then
            step = 1
        end
    end

    set_tint(loading_overlay, 0.0, 0.0, 0.0, 0.95)
    set_text(loading_text, "LOADING...")
    return load_scene("game/story.scene")
end

function StartStoryScene()
    if story_scene_loading then
        return false
    end

    story_scene_loading = true
    stop_bgm()
    return StartCoroutine("DelayedStartStoryScene")
end
