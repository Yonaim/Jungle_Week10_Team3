local Config = {
    debug = {
        enable_log = true,
        score_log_interval = 1.0,
    },

    player = {
        max_hp = 3,
        invincible_time = 1.0,

        forward_speed = 10.0,
        max_move_step = 0.25,
        lane_width = 4.0,
        lane_count = 3,
        lane_change_speed = 12.0,

        gravity = -25.0,
        jump_power = 10.0,
        slide_duration = 0.45,

        fall_dead_z = -10.0,

        ground_probe_distance = 5.0,
        ground_snap_distance = 0.25,
        skin_width = 0.05,
        fallback_half_height = 1.0,
    },

    camera = {
        relative_x = -8.0,
        relative_y = 0.0,
        relative_z = 5.0,
        look_ahead = 5.0,
        look_height = 1.5,
    },

    score = {
        distance_weight = 10.0,
        survival_time_weight = 5.0,
    },

    obstacle = {
        default_damage = 1,
    },

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

    map = {
        road_tile_length = 20.0,
        road_tile_count = 5,
        road_recycle_behind_distance = 20.0,

        obstacle_spawn_ahead_distance = 40.0,
        obstacle_spacing = 10.0,
        obstacle_recycle_behind_distance = 20.0,
    },
}

-- Compatibility aliases for older prototype scripts and editor property defaults.
Config.forward_speed = Config.player.forward_speed
Config.max_move_step = Config.player.max_move_step
Config.lane_width = Config.player.lane_width
Config.lane_count = Config.player.lane_count
Config.lane_change_speed = Config.player.lane_change_speed
Config.gravity = Config.player.gravity
Config.jump_power = Config.player.jump_power
Config.slide_duration = Config.player.slide_duration

Config.road_tile_length = Config.map.road_tile_length
Config.road_tile_count = Config.map.road_tile_count
Config.road_recycle_behind_distance = Config.map.road_recycle_behind_distance
Config.obstacle_spawn_ahead_distance = Config.map.obstacle_spawn_ahead_distance
Config.obstacle_spacing = Config.map.obstacle_spacing
Config.obstacle_recycle_behind_distance = Config.map.obstacle_recycle_behind_distance

Config.camera_back_distance = -Config.camera.relative_x
Config.camera_height = Config.camera.relative_z
Config.camera_follow_speed = 0.0
Config.camera_look_ahead = Config.camera.look_ahead
Config.camera_look_height = Config.camera.look_height

return Config
