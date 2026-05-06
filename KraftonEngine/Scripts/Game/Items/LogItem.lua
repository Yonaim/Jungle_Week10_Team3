local ItemBase = require("Game.Items.ItemBase")
local ItemsConfig = require("Game.Config.Items")

local LogFragmentConfig = ItemsConfig.log_fragment

local item = ItemBase.New({
    ScoreValue = LogFragmentConfig.score_value,
    RequiredInteractorTag = ItemsConfig.required_interactor_tag,
    Features = LogFragmentConfig.features,
})

------------------------------------------------
-- Log Fragment 충돌 콜백 함수들
------------------------------------------------

function OnBeginOverlap(otherActor, otherComp, selfComp)
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
    -- 지금은 끝 overlap에서 할 일이 없습니다. 나중에 pickup 범위 UI를 끌 때 쓰면 됨.
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    -- 지금은 Hit가 아니라 BeginOverlap으로만 먹습니다.
end
