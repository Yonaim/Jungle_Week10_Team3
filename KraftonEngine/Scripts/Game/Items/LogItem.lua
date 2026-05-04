local ItemBase = require("Game.Items.ItemBase")

local Config = require("Game.Config")

-- Log Fragment입니다. 여기서 logs/score/trace/coach approval을 올리면 됨.
DeclareProperties({
    -- RequiredInteractorTag는 어떤 actor가 이 Log Fragment를 먹을 수 있는지 정합니다.
    { name = "RequiredInteractorTag", type = "string", default = "Player" },
})

-- item은 ItemBase 공통 pickup 객체입니다.
-- LogFragmentReward feature가 켜져 있어서 overlap 시 GameManager.CollectLogFragment()가 호출됩니다.
local item = ItemBase.New({
    -- Log Fragment 점수는 Config.collectible.log_fragment_score에서만 관리합니다.
    ScoreValue = Config.collectible.log_fragment_score,
    RequiredInteractorTag = property("RequiredInteractorTag", "Player"),
    Features = {
        PickupOnOverlap = true,
        ConsumeOnPickup = true,
        ScoreReward = false,
        LogFragmentReward = true,
        SingleUse = true,
        DebugLog = true,
    },
})

function OnBeginOverlap(otherActor, otherComp, selfComp)
    -- ScriptComponent가 C++ overlap event를 이 전역 함수로 전달합니다.
    -- 실제 판정/Log Fragment 보상/소멸 순서는 ItemBase가 처리합니다.
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
    -- 지금은 끝 overlap에서 할 일이 없습니다. 나중에 pickup 범위 UI를 끌 때 쓰면 됨.
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    -- 지금은 Hit가 아니라 BeginOverlap으로만 먹습니다.
end
