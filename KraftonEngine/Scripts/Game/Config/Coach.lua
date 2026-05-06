local CoachConfig = {
    -- initial_approval: 게임 시작 시 코치 평가 초기값입니다.
    initial_approval = 50,

    -- *_delta: 게임 이벤트별 코치 평가 변화량입니다.
    log_collected_delta = 1,
    hotfix_delta = 8,
    critical_analysis_delta = 10,
    obstacle_avoided_delta = 1,
    player_hit_delta = -5,

    -- rank_thresholds: approval 점수 이상일 때 적용할 랭크 기준입니다.
    rank_thresholds = {
        { rank = "S", approval = 90 },
        { rank = "A", approval = 75 },
        { rank = "B", approval = 55 },
        { rank = "C", approval = 35 },
        { rank = "D", approval = 15 },
    },

    -- fallback_rank: 어떤 기준에도 들어가지 않을 때의 기본 랭크입니다.
    fallback_rank = "F",
}

return CoachConfig
