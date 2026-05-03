DeclareProperties({
    NextScene = { type = "string", default = "Presentation_FPS" },
    AutoAdvanceDelay = { type = "float", default = 3.0, min = 0.0, max = 10.0 },
})

local NEXT_SCENE = property("NextScene", "Presentation_FPS")
local AUTO_ADVANCE_DELAY = property("AutoAdvanceDelay", 3.0)

local ui = {}
local intro_finished = false

local function get_component(name)
    local component = obj:GetComponent(name)
    if component and component:IsValid() then
        return component
    end

    warn("StoryScene 컴포넌트를 찾을 수 없습니다:", name)
    return nil
end

local function cache_component(name)
    ui[name] = get_component(name)
end

local function set_visible(name, visible)
    local component = ui[name]
    if component then
        component:SetVisible(visible)
    end
end

local function set_text(name, text)
    local component = ui[name]
    if component then
        component:SetText(text)
    end
end

local function set_tint(name, r, g, b, a)
    local component = ui[name]
    if component then
        component:SetTint(r, g, b, a)
    end
end

local function set_texture(name, texture_path)
    local component = ui[name]
    if component then
        component:SetTexture(texture_path)
    end
end

local function hide_lines(prefix, count)
    local index = 1
    while index <= count do
        local name = prefix .. tostring(index)
        set_text(name, "")
        set_visible(name, false)
        index = index + 1
    end
end

local function set_line_group(prefix, count, lines, r, g, b, a)
    local index = 1
    while index <= count do
        local name = prefix .. tostring(index)
        local text = lines[index]
        if text and text ~= "" then
            set_text(name, text)
            set_tint(name, r, g, b, a)
            set_visible(name, true)
        else
            set_text(name, "")
            set_visible(name, false)
        end
        index = index + 1
    end
end

local function clear_all_text()
    hide_lines("BootLine", 4)
    hide_lines("NarrationLine", 6)
    hide_lines("SystemLine", 4)
    hide_lines("DialogueLine", 4)
    hide_lines("MissionLine", 5)

    set_text("BootHeader", "")
    set_visible("BootHeader", false)
    set_text("SpeakerName", "")
    set_visible("SpeakerName", false)
    set_text("MissionTitle", "")
    set_visible("MissionTitle", false)
    set_text("CountdownText", "")
    set_visible("CountdownText", false)
    set_text("StatusText", "")
    set_visible("StatusText", false)
end

local function hide_overlays()
    set_visible("Logo", false)
    set_visible("CommsPanel", false)
    set_visible("Portrait", false)
    set_visible("DebugIconGrid", false)
    set_visible("DebugIconWire", false)
    set_visible("DebugIconGizmo", false)
    set_visible("DebugIconPanel", false)
    set_visible("RedOverlay", false)
    set_visible("WhiteFlash", false)
end

local function reset_scene()
    hide_overlays()
    clear_all_text()

    set_visible("BgBase", true)
    set_visible("BgTexture", true)
    set_tint("BgBase", 0.01, 0.02, 0.04, 1.0)
    set_tint("BgTexture", 0.05, 0.09, 0.16, 0.32)
    set_tint("RedOverlay", 1.0, 0.1, 0.1, 0.0)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 0.0)
    set_tint("Logo", 1.0, 1.0, 1.0, 0.0)
    set_tint("CommsPanel", 0.72, 0.9, 1.0, 0.95)
    set_tint("Portrait", 1.0, 1.0, 1.0, 1.0)

    set_text("SkipHint", "[ESC] 건너뛰기")
    set_tint("SkipHint", 0.55, 0.72, 0.95, 0.9)
    set_visible("SkipHint", true)

    set_line_group(
        "HudLine",
        4,
        {
            "STABILITY 100%",
            "TRACE 0%",
            "LOGS 0",
            "COACH APPROVAL C",
        },
        0.55, 0.86, 1.0, 1.0)

    local hud_index = 1
    while hud_index <= 4 do
        set_visible("HudLine" .. tostring(hud_index), false)
        hud_index = hud_index + 1
    end
end

local function show_boot_sequence()
    set_text("BootHeader", "JUNGLE TECH LAB 내부 엔진")
    set_tint("BootHeader", 0.54, 0.86, 1.0, 1.0)
    set_visible("BootHeader", true)

    local boot_lines = {
        "BOOTING JUNGLE TECH LAB INTERNAL ENGINE...",
        "LOADING TRAINING SIMULATION...",
        "CHECKING RUNTIME STATE...",
        "READYING INTERNAL DEBUG CHANNEL..."
    }

    local index = 1
    while index <= 4 do
        set_text("BootLine" .. tostring(index), boot_lines[index])
        set_tint("BootLine" .. tostring(index), 0.40, 0.78, 1.0, 1.0)
        set_visible("BootLine" .. tostring(index), true)
        wait(0.38)
        index = index + 1
    end

    set_visible("Logo", true)
    set_tint("Logo", 1.0, 1.0, 1.0, 1.0)
    wait(0.95)
end

local function show_narration()
    set_line_group(
        "NarrationLine",
        6,
        {
            "JUNGLE Tech Lab.",
            "교육생들이 직접 만든 게임 엔진을 기반으로,",
            "실전형 개발 훈련을 진행하는 기술 교육 기관.",
            "렌더링, 충돌, 런타임, 디버깅까지",
            "모든 시스템이 교육생의 실력을 검증하는 훈련장이 된다.",
            "이곳에서 엔진은 단순한 과제가 아니다.",
        },
        0.93, 0.97, 1.0, 1.0)

    set_visible("DebugIconGrid", true)
    set_visible("DebugIconWire", true)
    set_visible("DebugIconGizmo", true)
    set_visible("DebugIconPanel", true)

    set_line_group(
        "SystemLine",
        4,
        {
            "STATIC MESH SYSTEM ONLINE",
            "MATERIAL SYSTEM ONLINE",
            "RUNTIME MODULE ONLINE",
            "DEBUG TRAINING MODE READY",
        },
        0.40, 0.78, 1.0, 1.0)

    wait(1.35)
end

local function show_failure()
    set_tint("BgTexture", 0.10, 0.06, 0.06, 0.28)
    set_visible("RedOverlay", true)
    set_tint("RedOverlay", 0.95, 0.12, 0.12, 0.18)

    set_line_group(
        "SystemLine",
        4,
        {
            "[ERROR] UNKNOWN RUNTIME FAILURE",
            "[ERROR] TRACE SOURCE NOT FOUND",
            "[WARNING] EXTERNAL DEBUGGER ACCESS DENIED",
            "[CRITICAL] INTERNAL ENGINE STATE UNSTABLE",
        },
        1.0, 0.34, 0.34, 1.0)

    play_sfx("Sound.SFX.canceled.sound.effect", false)
    wait(1.15)
end

local function show_dialogue(portrait_path, speaker, lines, speaker_r, speaker_g, speaker_b)
    set_visible("CommsPanel", true)
    set_visible("Portrait", true)
    set_texture("Portrait", portrait_path)

    set_text("SpeakerName", speaker)
    set_tint("SpeakerName", speaker_r, speaker_g, speaker_b, 1.0)
    set_visible("SpeakerName", true)

    set_line_group("DialogueLine", 4, lines, 0.95, 0.97, 1.0, 1.0)
end

local function show_commander_sequence()
    hide_lines("NarrationLine", 6)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_baek_0.png",
        "백승현 사령관",
        {
            "“JUNGLE Tech Lab 내부 엔진에서",
            "원인 불명의 런타임 오류가 감지됐다.”",
            "“외부 로그 분석과 일반 디버거로는",
            "원인을 추적할 수 없다.”",
        },
        0.62, 0.86, 1.0)
    wait(2.2)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_baek_1.png",
        "백승현 사령관",
        {
            "“오류는 엔진 내부 깊은 런타임 구간에서 발생하고 있으며,”",
            "“시간이 지날수록 시스템 안정도가 하락하고 있다.”",
            "“따라서 지금부터 교육생을 직접 투입한다.”",
            "",
        },
        0.62, 0.86, 1.0)

    set_line_group(
        "SystemLine",
        4,
        {
            "DEBUG POD: STANDBY",
            "PILOT SYNC: READY",
            "MISSION TYPE: INTERNAL ENGINE DIVE",
            "",
        },
        0.40, 0.78, 1.0, 1.0)
    wait(2.0)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_cg_0.png",
        "임창근 사령관",
        {
            "“쉽게 말하면, 네가 직접 엔진 안으로 들어가서”",
            "“어디서 터지는지 보고 오라는 뜻이다.”",
            "“걱정 마라. 포드는 튼튼하다.”",
            "“아마도.”",
        },
        1.0, 0.77, 0.35)
    wait(2.0)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_baek_1.png",
        "백승현 사령관",
        {
            "“불필요한 설명은 줄이도록.”",
            "",
            "",
            "",
        },
        0.62, 0.86, 1.0)
    wait(1.2)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_cg_1.png",
        "임창근 사령관",
        {
            "“긴장 풀어. 검은 화면이든, 그리드든, 버그든,”",
            "“오류 패널이든 아주 친절하게 반겨줄 거다.”",
            "",
            "",
        },
        1.0, 0.77, 0.35)
    wait(1.8)
end

local function show_mission()
    set_text("MissionTitle", "MISSION OBJECTIVE")
    set_tint("MissionTitle", 1.0, 0.92, 0.58, 1.0)
    set_visible("MissionTitle", true)

    set_line_group(
        "MissionLine",
        5,
        {
            "1. COLLECT LOG FRAGMENTS",
            "2. SECURE TRACE DATA",
            "3. APPLY HOTFIX",
            "4. SURVIVE AS LONG AS POSSIBLE",
            "",
        },
        0.88, 0.95, 1.0, 1.0)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_baek_0.png",
        "백승현 사령관",
        {
            "“엔진 내부를 질주하며 Log 조각을 수집하고,”",
            "“Trace Data를 확보해 오류의 근원을 추적해라.”",
            "“Trace가 충분히 쌓이면 Hotfix를 적용할 수 있다.”",
            "“Stability가 0%가 되면 세션은 강제 종료된다.”",
        },
        0.62, 0.86, 1.0)
    wait(2.5)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_cg_0.png",
        "임창근 사령관",
        {
            "“즉, 로그는 먹고, 오류는 피하고,”",
            "“터지기 전에 고치면 된다.”",
            "“아주 간단하지. 물론 말로만.”",
            "",
        },
        1.0, 0.77, 0.35)
    wait(2.0)
end

local function show_launch_sequence()
    local hud_index = 1
    while hud_index <= 4 do
        set_visible("HudLine" .. tostring(hud_index), true)
        hud_index = hud_index + 1
    end

    show_dialogue(
        "Asset/Content/EngineDive/portrait_baek_1.png",
        "백승현 사령관",
        {
            "“교육생, 동기화 상태 확인.”",
            "“지금부터 네 기록은 훈련 시스템에 저장된다.”",
            "“가능한 한 깊은 런타임 구간까지 진입해라.”",
            "",
        },
        0.62, 0.86, 1.0)
    wait(2.0)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_cg_1.png",
        "임창근 사령관",
        {
            "“그리고 가능하면 멋있게 해라.”",
            "“코치 인정도는 생각보다 냉정하다.”",
            "",
            "",
        },
        1.0, 0.77, 0.35)
    wait(1.8)

    set_text("StatusText", "ENGINE DIVE SEQUENCE START")
    set_tint("StatusText", 0.58, 0.88, 1.0, 1.0)
    set_visible("StatusText", true)

    local countdown = { "3", "2", "1" }
    local index = 1
    while index <= 3 do
        set_text("CountdownText", countdown[index])
        set_tint("CountdownText", 1.0, 1.0, 1.0, 1.0)
        set_visible("CountdownText", true)
        play_sfx("Sound.SFX.go", false)
        wait(0.72)
        index = index + 1
    end

    show_dialogue(
        "Asset/Content/EngineDive/portrait_baek_0.png",
        "백승현 사령관",
        {
            "“디버그 포드, 투입.”",
            "",
            "",
            "",
        },
        0.62, 0.86, 1.0)
    wait(0.8)

    show_dialogue(
        "Asset/Content/EngineDive/portrait_cg_0.png",
        "임창근 사령관",
        {
            "“자, 엔진 속으로 뛰어들어라!”",
            "",
            "",
            "",
        },
        1.0, 0.77, 0.35)
    wait(0.8)

    set_visible("WhiteFlash", true)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 1.0)
    set_text("CountdownText", "ENGINE DIVE")
    set_text("StatusText", "SESSION START")
    wait(0.35)
    set_tint("WhiteFlash", 1.0, 1.0, 1.0, 0.0)

    local elapsed = 0.0
    local blink_visible = true
    while elapsed < AUTO_ADVANCE_DELAY do
        if blink_visible then
            set_tint("StatusText", 0.58, 0.88, 1.0, 1.0)
            set_tint("CountdownText", 1.0, 1.0, 1.0, 1.0)
        else
            set_tint("StatusText", 0.58, 0.88, 1.0, 0.25)
            set_tint("CountdownText", 1.0, 1.0, 1.0, 0.25)
        end

        blink_visible = not blink_visible
        wait(0.3)
        elapsed = elapsed + 0.3
    end
end

local function finish_intro()
    if intro_finished then
        return
    end

    intro_finished = true
    stop_bgm()
    load_scene(NEXT_SCENE)
end

function RunIntro()
    reset_scene()
    stop_bgm()
    play_bgm("Game.Sound.Background.Cutscene1", false)
    play_sfx("Sound.SFX.go", false)

    show_boot_sequence()
    show_narration()
    show_failure()
    show_commander_sequence()
    show_mission()
    show_launch_sequence()
    finish_intro()
end

function BeginPlay()
    local names = {
        "BgBase",
        "BgTexture",
        "RedOverlay",
        "WhiteFlash",
        "Logo",
        "CommsPanel",
        "Portrait",
        "DebugIconGrid",
        "DebugIconWire",
        "DebugIconGizmo",
        "DebugIconPanel",
        "BootHeader",
        "BootLine1",
        "BootLine2",
        "BootLine3",
        "BootLine4",
        "NarrationLine1",
        "NarrationLine2",
        "NarrationLine3",
        "NarrationLine4",
        "NarrationLine5",
        "NarrationLine6",
        "SystemLine1",
        "SystemLine2",
        "SystemLine3",
        "SystemLine4",
        "SpeakerName",
        "DialogueLine1",
        "DialogueLine2",
        "DialogueLine3",
        "DialogueLine4",
        "MissionTitle",
        "MissionLine1",
        "MissionLine2",
        "MissionLine3",
        "MissionLine4",
        "MissionLine5",
        "HudLine1",
        "HudLine2",
        "HudLine3",
        "HudLine4",
        "CountdownText",
        "StatusText",
        "SkipHint",
    }

    for _, name in ipairs(names) do
        cache_component(name)
    end

    StartCoroutine("RunIntro")
end

function Tick(dt)
    if intro_finished then
        return
    end

    if GetKeyDown("ESC") then
        finish_intro()
    end
end
