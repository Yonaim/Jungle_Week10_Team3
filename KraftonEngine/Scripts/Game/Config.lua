local Config = {
    -- debug는 개발 중 콘솔 로그를 켜고 끄는 묶음입니다.
    -- HUD/비주얼 작업자가 수치 연결을 확인할 때 enable_log를 잠깐 켜면 흐름을 보기 쉽습니다.
    debug = {
        -- enable_log는 Lua 시스템 로그 출력 여부입니다.
        enable_log = true,
        -- score_log_interval은 주기적으로 현재 점수/거리/시간을 찍는 간격입니다.
        score_log_interval = 1.0,
    },

    -- player는 Runner 조작과 생존 수치입니다.
    -- max_hp 값은 PlayerStatus에서 포드 안정도 최대치로 읽어 GameManager HUD 스냅샷과 연결합니다.
    player = {
        -- max_hp는 PlayerStatus 내부 저장 이름이고, 게임 규칙상 "포드 안정도 최대치"입니다.
        max_hp = 3,
        -- invincible_time은 피격 직후 같은 장애물에 여러 번 맞지 않도록 막는 시간입니다.
        invincible_time = 1.0,

        -- forward_speed는 자동 전진 속도입니다.
        forward_speed = 10.0,
        -- max_move_step은 빠른 이동 중 overlap 누락을 줄이기 위해 한 프레임 이동을 쪼개는 최대 길이입니다.
        max_move_step = 0.25,
        -- lane_width는 레인 사이 Y 간격입니다.
        lane_width = 4.0,
        -- lane_count는 좌/중/우 3레인 같은 총 레인 수입니다.
        lane_count = 3,
        -- lane_change_speed는 target lane까지 보간 이동하는 속도입니다.
        lane_change_speed = 12.0,
        barrel_roll_enabled = true,
        barrel_roll_degrees = 360.0,
        barrel_roll_duration = 0.34,

        -- gravity는 공중에 있을 때 매 프레임 더해지는 수직 가속도입니다.
        gravity = -25.0,
        -- jump_power는 점프 시작 순간 수직 속도입니다.
        jump_power = 10.0,
        -- 슬라이드는 duration 타이머가 아니라 입력을 누르고 있는 동안 유지합니다.
        -- 유지/종료 판단은 PlayerController.slide_key_pressed()와 PlayerSlide:Begin/End가 담당합니다.

        -- fall_dead_z 아래로 떨어지면 안정도와 무관하게 낙사 처리합니다.
        fall_dead_z = -10.0,

        -- ground_probe_distance는 발 아래로 바닥을 찾는 최대 거리입니다.
        ground_probe_distance = 5.0,
        -- ground_snap_distance는 바닥에 거의 붙었을 때만 보정하는 거리입니다.
        ground_snap_distance = 0.25,
        -- skin_width는 충돌/바닥 판정에 쓰는 아주 작은 여유값입니다.
        skin_width = 0.05,
        -- fallback_half_height는 collision shape를 못 찾았을 때 쓰는 임시 반높이입니다.
        fallback_half_height = 1.0,
    },

    -- camera는 따라오는 카메라 위치/시선 기본값입니다.
    camera = {
        relative_x = -8.0,
        relative_y = 0.0,
        relative_z = 5.0,
        look_ahead = 5.0,
        look_height = 1.5,
    },

    -- score는 달린 거리/생존 시간으로 올라가는 기본 점수 가중치입니다.
    -- 아이템 보너스는 GameManager.bonus_score에 따로 누적됩니다.
    score = {
        distance_weight = 10.0,
        survival_time_weight = 5.0,
    },

    -- collectible은 HUD/이펙트 작업자가 나중에 아이템 연출을 붙일 때 참고할 게임 규칙입니다.
    -- 실제 비주얼은 만들지 않고, 수치 변화와 발동 지점만 Lua에서 먼저 연결합니다.
    collectible = {
        -- Log Fragment 하나를 먹으면 올라가는 점수입니다.
        log_fragment_score = 10,
        -- Log Fragment 하나를 먹으면 trace가 이만큼 쌓입니다.
        log_fragment_trace = 2,
        -- trace가 이 값 이상이면 Hotfix가 발동됩니다.
        trace_max = 100,
        -- Hotfix 발동 보너스 점수입니다.
        hotfix_score = 1000,
        -- Hotfix 발동 시 포드 안정도를 이만큼 회복합니다.
        hotfix_stability_recover = 15,
        -- Crash Dump를 이 개수만큼 모으면 Critical Analysis가 발동됩니다.
        crash_dump_required = 3,
        -- Critical Analysis 발동 보너스 점수입니다.
        critical_analysis_score = 3000,
        -- Critical Analysis 발동 시 충돌 방어막을 이만큼 지급합니다.
        critical_analysis_shield_reward = 1,
    },

    -- coach는 코치 인정도와 랭크 계산에 쓰는 값입니다.
    -- HUD는 GameManager getter만 읽으면 되고, 여기 숫자만 바꾸면 밸런스 조정이 됩니다.
    coach = {
        -- initial_approval은 게임 시작 시 코치 인정도입니다.
        initial_approval = 50,
        -- log_collected_delta는 Log Fragment 획득 시 소폭 상승량입니다.
        log_collected_delta = 1,
        -- hotfix_delta는 Hotfix 성공 시 크게 올라가는 인정도입니다.
        hotfix_delta = 8,
        -- critical_analysis_delta는 Crash Dump 분석 성공 시 크게 올라가는 인정도입니다.
        critical_analysis_delta = 10,
        -- obstacle_avoided_delta는 장애물 회피 판정이 붙으면 올릴 값입니다. 지금은 호출 지점만 준비합니다.
        obstacle_avoided_delta = 1,
        -- near_miss_delta는 아슬아슬 회피 판정이 붙으면 올릴 값입니다. 지금은 TODO만 둡니다.
        near_miss_delta = 3,
        -- command_success_delta는 추후 명령 성공 이벤트가 생기면 올릴 값입니다.
        command_success_delta = 2,
        -- command_failed_delta는 추후 명령 실패 이벤트가 생기면 내릴 값입니다.
        command_failed_delta = -3,
        -- player_hit_delta는 장애물 충돌 시 내려가는 인정도입니다.
        player_hit_delta = -5,
    },

    -- obstacle은 장애물 피해량 기본값입니다.
    -- C++ Damage 바인딩이 없는 장애물은 이 값을 fallback으로 사용합니다.
    obstacle = {
        -- default_damage는 일반 장애물 기본 안정도 피해량입니다.
        default_damage = 1,
        -- high_risk_damage는 나중에 고위험 장애물 타입에 넣을 수 있는 기본값입니다.
        -- TODO: 장애물별 Damage 테이블을 Lua Config로 완전히 옮길 수 있으면 여기서 타입별로 관리하면 됨.
        high_risk_damage = 2,
    },

    -- audio는 현재 사운드 설정입니다. 이번 작업에서는 SFX/Audio 파일과 로직은 건드리지 않습니다.
    audio = {
        enabled = true,

        play_bgm = "Asset/Content/Sound/Background/03. Tutorial.mp3",

        hit_sfx = "",
        jump_sfx = "",
        slide_sfx = "",
        game_over_sfx = "",

        bgm_loop = true,
        stop_bgm_on_game_over = true,
    },

    -- map은 길/장애물 생성 간격입니다.
    result_screen = {
        scene_path = "game/gameresult.scene",
        scoreboard_path = "Saves/scoreboard.json",
        scoreboard_limit = 20,
        title = "DEBUG SESSION RESULT",
        coach_name_baek = "백승현 코치",
        coach_name_lim = "임창근 코치",
        coach_comment_baek = "런타임 구간에서 판단이 안정적이었다.",
        coach_comment_lim = "좋다. 이 정도면 버그가 먼저 도망가겠다.",
    },

    map = {
        road_tile_length = 20.0,
        road_tile_count = 5,
        road_recycle_behind_distance = 20.0,

        obstacle_spawn_ahead_distance = 40.0,
        obstacle_spacing = 10.0,
        obstacle_recycle_behind_distance = 20.0,
    },
}

-- Config는 도메인별 table(Config.player/map/camera/collectible)만 공개합니다.
-- 새 스크립트는 평면 alias 없이 필요한 묶음을 직접 읽어 값의 출처를 분명하게 유지합니다.

return Config
