local CollectibleConfig = {
    -- log_fragment_*: Log Fragment 획득 보상입니다.
    log_fragment_score = 10,
    log_fragment_trace = 2,

    -- trace_max: Hotfix 발동에 필요한 trace 최대값입니다.
    trace_max = 100,

    -- hotfix_*: Hotfix 발동 보상입니다.
    hotfix_score = 1000,
    hotfix_stability_recover = 15,

    -- crash_dump_required: Critical Analysis에 필요한 Crash Dump 개수입니다.
    crash_dump_required = 3,

    -- critical_analysis_*: Critical Analysis 발동 보상입니다.
    critical_analysis_score = 3000,
    critical_analysis_shield_reward = 1,
}

return CollectibleConfig
