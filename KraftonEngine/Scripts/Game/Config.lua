local Config = {
    -- debug: 개발/디버깅용 로그 출력 설정입니다.
    -- HUD나 콘솔에 너무 많은 로그가 찍히면 enable_log를 false로 꺼도 됩니다.
    debug = {
        -- enable_log: Lua 쪽 디버그 로그를 출력할지 여부입니다.
        enable_log = true,
        -- score_log_interval: 점수/생존 시간 로그를 몇 초 간격으로 출력할지 설정합니다.
        score_log_interval = 1.0,
    },

    -- player: Runner 플레이어의 기본 이동, 체력, 충돌 관련 설정입니다.
    -- max_hp는 PlayerStatus의 최대 체력이며, GameManager와 HUD 표시에도 사용됩니다.
    player = {
        -- max_hp: PlayerStatus 초기/최대 체력 값입니다.
        max_hp = 3,
        -- invincible_time: 피격 직후 추가 피해를 막는 무적 시간입니다.
        invincible_time = 1.0,

        -- forward_speed: 플레이어가 전방으로 자동 이동하는 속도입니다.
        forward_speed = 10.0,
        -- max_move_step: 한 프레임 이동량을 쪼개 overlap/충돌 누락을 줄이기 위한 최대 이동 스텝입니다.
        max_move_step = 0.25,
        -- lane_width: 각 레인의 Y축 간격입니다.
        lane_width = 4.0,
        -- lane_count: 총 레인 개수입니다. 기본값은 3레인입니다.
        lane_count = 3,
        -- lane_change_speed: 목표 레인까지 보간 이동하는 속도입니다.
        lane_change_speed = 12.0,
        -- barrel_roll_enabled: 레인 변경 시 배럴 롤 연출을 사용할지 여부입니다.
        barrel_roll_enabled = true,
        -- barrel_roll_degrees: 배럴 롤 회전 각도입니다.
        barrel_roll_degrees = 360.0,
        -- barrel_roll_duration: 배럴 롤 연출 지속 시간입니다.
        barrel_roll_duration = 0.34,

        -- gravity: 플레이어에게 적용되는 중력 가속도입니다.
        gravity = -25.0,
        -- jump_power: 점프 시작 시 적용되는 위쪽 속도입니다.
        jump_power = 10.0,
        -- slide는 별도 duration 기반 상태로 관리하는 것을 권장합니다.
        -- 입력/해제 처리는 PlayerController.slide_key_pressed()와 PlayerSlide:Begin/End 쪽 흐름을 확인하세요.

        -- fall_dead_z: 플레이어가 이 Z값 아래로 떨어지면 사망 처리하는 기준 높이입니다.
        fall_dead_z = -10.0,

        -- ground_probe_distance: 바닥 감지를 위해 아래 방향으로 검사하는 거리입니다.
        ground_probe_distance = 5.0,
        -- ground_snap_distance: 바닥에 가까울 때 위치를 바닥에 붙이는 보정 거리입니다.
        ground_snap_distance = 0.25,
        -- skin_width: 충돌체와 지형 사이에 남겨두는 작은 여유 간격입니다.
        skin_width = 0.05,
        -- fallback_half_height: collision shape 정보를 얻지 못했을 때 사용할 기본 반높이입니다.
        fallback_half_height = 1.0,
    },

    -- camera: 플레이어를 따라가는 기본 카메라 상대 위치/시선 설정입니다.
    camera = {
        relative_x = -8.0,
        relative_y = 0.0,
        relative_z = 5.0,
        look_ahead = 5.0,
        look_height = 1.5,
    },

    -- score: 거리와 생존 시간을 기반으로 점수를 계산할 때 사용하는 가중치입니다.
    -- 추가 점수는 GameManager.bonus_score 같은 별도 값으로 더해지는 구조를 권장합니다.
    score = {
        distance_weight = 10.0,
        survival_time_weight = 5.0,
    },

    -- collectible: HUD/점수/trace와 연결되는 수집 아이템 관련 설정입니다.
    -- 아이템 획득 효과는 Lua 또는 C++ 아이템 로직에서 이 값을 참조해 처리합니다.
    collectible = {
        -- Log Fragment 획득 시 추가되는 점수입니다.
        log_fragment_score = 10,
        -- Log Fragment 획득 시 증가하는 trace 값입니다.
        log_fragment_trace = 2,
        -- trace의 최대값입니다. Hotfix 등으로 관리됩니다.
        trace_max = 100,
        -- Hotfix 획득 시 추가되는 점수입니다.
        hotfix_score = 1000,
        -- Hotfix 획득 시 회복되는 안정성/체력 관련 값입니다.
        hotfix_stability_recover = 15,
        -- Crash Dump를 몇 개 모아야 Critical Analysis가 가능한지 정합니다.
        crash_dump_required = 3,
        -- Critical Analysis 성공 시 추가되는 점수입니다.
        critical_analysis_score = 3000,
        -- Critical Analysis 성공 시 지급되는 실드 보상 개수입니다.
        critical_analysis_shield_reward = 1,
    },

    -- coach: 코치 호감도/평가 수치 변화량 설정입니다.
    -- HUD는 GameManager getter를 통해 이 값을 읽어 표시하는 구조를 권장합니다.
    coach = {
        -- initial_approval: 시작 시 코치 평가/호감도 초기값입니다.
        initial_approval = 50,
        -- log_collected_delta: Log Fragment 획득 시 평가 변화량입니다.
        log_collected_delta = 1,
        -- hotfix_delta: Hotfix 획득 시 평가 변화량입니다.
        hotfix_delta = 8,
        -- critical_analysis_delta: Crash Dump 분석 성공 시 평가 변화량입니다.
        critical_analysis_delta = 10,
        -- obstacle_avoided_delta: 장애물을 회피했을 때 평가 변화량입니다. 실제 호출 지점은 별도 구현이 필요할 수 있습니다.
        obstacle_avoided_delta = 1,
        -- near_miss_delta: 아슬아슬하게 장애물을 회피했을 때 평가 변화량입니다. TODO 구현 항목입니다.
        near_miss_delta = 3,
        -- command_success_delta: 명령 수행 성공 시 평가 변화량입니다.
        command_success_delta = 2,
        -- command_failed_delta: 명령 수행 실패 시 평가 변화량입니다.
        command_failed_delta = -3,
        -- player_hit_delta: 플레이어 피격 시 평가 변화량입니다.
        player_hit_delta = -5,
    },

    -- obstacle: 장애물 피해량 관련 기본 설정입니다.
    -- C++ Damage 컴포넌트 값이 없을 때 fallback 값으로 사용할 수 있습니다.
    obstacle = {
        -- default_damage: 기본 장애물 피해량입니다.
        default_damage = 1,
        -- high_risk_damage: 고위험 장애물의 피해량입니다.
        -- TODO: 장애물별 Damage 값을 Lua Config에서 읽도록 연결할 수 있습니다.
        high_risk_damage = 2,
    },

    -- audio: 게임플레이 BGM/SFX 경로 및 재생 옵션 설정입니다.
    -- 실제 재생은 SFX/Audio 시스템에서 이 경로를 읽어 처리합니다.
    audio = {
        enabled = true,

        play_bgm = "Asset/Content/Sound/Background/gameplay.mp3",

        hit_sfx = "",
        jump_sfx = "",
        slide_sfx = "",
        game_over_sfx = "",

        bgm_loop = true,
        stop_bgm_on_game_over = true,
    },

    -- result_screen: 게임 결과 화면에서 사용할 씬/스코어보드/문구 설정입니다.
    result_screen = {
        scene_path = "game/gameresult.scene",
        scoreboard_path = "Saves/scoreboard.json",
        scoreboard_limit = 20,
        title = "DEBUG SESSION RESULT",
        coach_name_baek = "백 사령관",
        coach_name_lim = "임 오퍼레이터",
        coach_comment_baek = "판단은 정확했다. 다음엔 더 깊이 들어가라.",
        coach_comment_lim = "나쁘지 않다. 이제 버그가 널 좀 무서워하겠군.",
    },

    -- map: 도로 타일과 장애물 스폰/재활용 거리 설정입니다.
    map = {
        road_tile_length = 20.0,
        road_tile_count = 5,
        road_recycle_behind_distance = 20.0,

        obstacle_spawn_ahead_distance = 40.0,
        obstacle_spacing = 10.0,
        obstacle_recycle_behind_distance = 20.0,
    },
}

-- Config를 외부 Lua 파일에서 require한 뒤 Config.player/map/camera/collectible처럼 접근해서 사용합니다.
-- 기존 코드와 호환되도록 최상위 table 하나만 return합니다.

return Config
