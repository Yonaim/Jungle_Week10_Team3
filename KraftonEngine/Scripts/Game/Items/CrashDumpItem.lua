local ItemBase = require("Game.Items.ItemBase")

-- CrashDumpItem은 Crash Dump 수집 규칙만 연결하는 Lua 아이템입니다.
-- HUD/비주얼 담당자는 dumps가 3개 됐을 때 GameManager.ApplyCriticalAnalysis 지점에 연출을 붙이면 됩니다.
DeclareProperties({
    -- RequiredInteractorTag는 어떤 actor가 Crash Dump를 먹을 수 있는지 정합니다.
    { name = "RequiredInteractorTag", type = "string", default = "Player" },
})

-- item은 ItemBase 공통 pickup 객체입니다.
-- CrashDumpReward feature가 켜져 있어서 overlap 시 dumps가 쌓이고 3개째에 Critical Analysis가 발동됩니다.
local item = ItemBase.New({
    RequiredInteractorTag = property("RequiredInteractorTag", "Player"),
    Features = {
        PickupOnOverlap = true,
        ConsumeOnPickup = true,
        CrashDumpReward = true,
        SingleUse = true,
        DebugLog = true,
    },
})

function OnBeginOverlap(otherActor, otherComp, selfComp)
    -- Crash Dump 획득 시작점입니다. 실제 수치/소멸 처리는 ItemBase가 담당합니다.
    item:OnBeginOverlap(otherActor, otherComp, selfComp)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
    -- 지금은 끝 overlap에서 할 일이 없습니다. 나중에 근접 HUD를 끌 때 쓰면 됨.
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    -- 지금은 Hit가 아니라 BeginOverlap으로만 먹습니다.
end
