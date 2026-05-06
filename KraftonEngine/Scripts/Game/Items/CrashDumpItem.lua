local ItemBase = require("Game.Items.ItemBase")
local ItemsConfig = require("Game.Config.Items")

local item = ItemBase.New({
    RequiredInteractorTag = ItemsConfig.required_interactor_tag,
    Features = ItemsConfig.crash_dump.features,
})

------------------------------------------------
-- Crash Dump 충돌 콜백 함수들
------------------------------------------------

function OnBeginOverlap(otherActor, otherComp, selfComp)
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
    -- 지금은 끝 overlap에서 할 일이 없습니다. 나중에 근접 HUD를 끌 때 쓰면 됨.
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    -- 지금은 Hit가 아니라 BeginOverlap으로만 먹습니다.
end
