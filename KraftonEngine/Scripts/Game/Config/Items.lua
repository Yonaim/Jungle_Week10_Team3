local CollectibleConfig = require("Game.Config.Collectible")

local ItemsConfig = {
    -- required_interactor_tag: 기본 아이템을 획득할 수 있는 actor tag입니다.
    required_interactor_tag = "Player",

    -- coin: 단순 점수 보상 아이템 설정입니다.
    coin = {
        score_value = 10,
        features = {
            PickupOnOverlap = true,
            ConsumeOnPickup = true,
            ScoreReward = true,
            SingleUse = true,
            DebugLog = true,
        },
    },

    -- log_fragment: trace/score를 올리는 Log Fragment 아이템 설정입니다.
    log_fragment = {
        score_value = CollectibleConfig.log_fragment_score,
        features = {
            PickupOnOverlap = true,
            ConsumeOnPickup = false,
            ScoreReward = false,
            LogFragmentReward = true,
            SingleUse = true,
            DebugLog = true,
        },
    },

    -- crash_dump: Critical Analysis 재료 아이템 설정입니다.
    crash_dump = {
        features = {
            PickupOnOverlap = true,
            ConsumeOnPickup = false,
            CrashDumpReward = true,
            SingleUse = true,
            DebugLog = true,
        },
    },
}

return ItemsConfig
