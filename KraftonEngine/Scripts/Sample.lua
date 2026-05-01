-- Sample.lua
-- 현재 Lua 바인딩으로 사용할 수 있는 기능을 한 번씩 호출해 보는 통합 샘플이다.
-- wait 계열 함수는 반드시 StartCoroutine/start_coroutine으로 시작한 코루틴 내부에서만 사용한다.

Sample = {}

local spawned_actor = nil
local tick_logged = false

local function vector_to_string(v)
    return "(" .. tostring(v.x) .. ", " .. tostring(v.y) .. ", " .. tostring(v.z) .. ")"
end

local function color_to_string(c)
    return "(" .. tostring(c.r) .. ", " .. tostring(c.g) .. ", " .. tostring(c.b) .. ", " .. tostring(c.a) .. ")"
end

local function log_actor_state(prefix, actor)
    if actor:IsValid() then
        log(prefix .. " valid name=" .. actor.Name .. " uuid=" .. tostring(actor.UUID))
        log(prefix .. " location=" .. vector_to_string(actor.Location))
        log(prefix .. " rotation=" .. vector_to_string(actor.Rotation))
        log(prefix .. " scale=" .. vector_to_string(actor.Scale))
        log(prefix .. " velocity=" .. vector_to_string(actor.Velocity))
    else
        warn(prefix .. " invalid")
    end
end

function Sample.TestMathTypes()
    local a = vec3(1, 2, 3)
    local b = vec3(4, 5, 6)
    local sum = a + b
    local diff = b - a
    local scaled = a * 2
    local divided = b / 2

    a.x = 10
    a.y = 20
    a.z = 30

    log("[Sample] Vector a=" .. vector_to_string(a))
    log("[Sample] Vector sum=" .. vector_to_string(sum))
    log("[Sample] Vector diff=" .. vector_to_string(diff))
    log("[Sample] Vector scaled=" .. vector_to_string(scaled))
    log("[Sample] Vector divided=" .. vector_to_string(divided))

    local c1 = Color.new(255, 0, 0, 255)
    local c2 = MakeColor(0, 128, 255, 255)
    c1.g = 64

    log("[Sample] Color c1=" .. color_to_string(c1))
    log("[Sample] Color c2=" .. color_to_string(c2))
end

function Sample.TestComponents(actor)
    local by_name = actor:GetComponent("StaticMeshComponent")
    local by_type = actor:GetComponentByType("UStaticMeshComponent")
    local mesh = actor:GetStaticMeshComponent()
    local script = actor:GetScriptComponent()

    if by_name:IsValid() then
        log("[Sample] GetComponent found: " .. by_name.Name)
        log("[Sample] Component active before=" .. tostring(by_name:IsActive()))
        by_name:SetActive(false)
        log("[Sample] Component active after false=" .. tostring(by_name:IsActive()))
        by_name:SetActive(true)
        log("[Sample] Component active after true=" .. tostring(by_name:IsActive()))

        local owner = by_name.Owner
        if owner:IsValid() then
            log("[Sample] Component owner=" .. owner.Name)
        end
    else
        warn("[Sample] StaticMeshComponent not found by name")
    end

    log("[Sample] GetComponentByType valid=" .. tostring(by_type:IsValid()))
    log("[Sample] GetStaticMeshComponent valid=" .. tostring(mesh:IsValid()))
    log("[Sample] GetScriptComponent valid=" .. tostring(script:IsValid()))
end

function Sample.TestActorProxy()
    if not obj:IsValid() then
        error_log("[Sample] obj is invalid")
        return
    end

    log_actor_state("[Sample] obj", obj)

    obj.Location = obj.Location + vec3(10, 0, 0)
    obj.Rotation = vec3(0, 0, 0)
    obj.Scale = vec3(1, 1, 1)
    obj.Velocity = vec3(0, 0, 0)

    obj:AddWorldOffset(vec3(0, 10, 0))
    obj:AddWorldOffset(0, -10, 0)
    obj:PrintLocation()

    obj:SetMoveSpeed(250)
    log("[Sample] move speed=" .. tostring(obj:GetMoveSpeed()))
    log("[Sample] move done before move=" .. tostring(obj:IsMoveDone()))

    Sample.TestComponents(obj)
end

function Sample.TestWorldFunctions()
    spawned_actor = spawn_actor("AStaticMeshActor", obj.Location + vec3(120, 0, 0))

    if spawned_actor:IsValid() then
        log_actor_state("[Sample] spawned", spawned_actor)
        spawned_actor:SetMoveSpeed(200)
        spawned_actor:MoveToActor(obj)

        local found = find_actor(spawned_actor.Name)
        log("[Sample] find_actor spawned valid=" .. tostring(found:IsValid()))
    else
        warn("[Sample] spawn_actor failed")
    end
end

function Sample.CoroutineTest()
    log("[Sample] CoroutineTest start")

    wait_frames(1)
    log("[Sample] wait_frames done")

    wait(0.1)
    log("[Sample] wait done")

    Wait(0.1)
    log("[Sample] Wait alias done")

    obj:MoveTo(obj.Location + vec3(30, 0, 0))
    wait_until_move_done()
    log("[Sample] wait_until_move_done done")

    obj:MoveBy(vec3(-30, 0, 0))
    wait_until_move_done()
    log("[Sample] MoveBy vector done")

    obj:MoveBy(10, 0)
    wait_until_move_done()
    log("[Sample] MoveBy 2D done")

    obj:MoveBy(-10, 0, 0)
    wait_until_move_done()
    log("[Sample] MoveBy 3D done")

    obj:MoveTo(obj.Location + vec3(0, 20, 0))
    obj:StopMove()
    log("[Sample] StopMove called, IsMoveDone=" .. tostring(obj:IsMoveDone()))

    obj:PlayAnimation("SampleAnim")
    log("[Sample] anim done before wait=" .. tostring(obj:IsAnimationDone("SampleAnim")))
    wait_anim_done("SampleAnim")
    log("[Sample] wait_anim_done done")

    if spawned_actor ~= nil and spawned_actor:IsValid() then
        spawned_actor:StopMove()
        destroy_actor(spawned_actor)
        log("[Sample] destroy_actor called for spawned actor")
    end

    log("[Sample] CoroutineTest end")
end

function Sample.SignalWaitTest()
    log("[Sample] SignalWaitTest waiting")
    wait_signal("SampleSignal")
    log("[Sample] SignalWaitTest received")
end

function Sample.KeyWaitTest()
    log("[Sample] KeyWaitTest waiting SPACE")
    wait_key_down("SPACE")
    log("[Sample] SPACE was pressed")
end

function BeginPlay()
    log("[Sample] BeginPlay")

    Sample.TestMathTypes()
    Sample.TestActorProxy()
    Sample.TestWorldFunctions()

    StartCoroutine("Sample.CoroutineTest")
    start_coroutine("Sample.SignalWaitTest")
    start_coroutine("Sample.KeyWaitTest")
end

function Tick(dt)
    if not tick_logged then
        tick_logged = true
        log("[Sample] Tick dt=" .. tostring(dt))
        log("[Sample] time=" .. tostring(time()) .. " delta_time=" .. tostring(delta_time()))
        print("[Sample] print alias works")
    end

    if key_down("SPACE") or GetKeyDown("SPACE") then
        signal("SampleSignal")
    end

    if key("LEFT") or GetKey("LEFT") then
        obj:AddWorldOffset(-100 * dt, 0, 0)
    end

    if key("RIGHT") or GetKey("RIGHT") then
        obj:AddWorldOffset(100 * dt, 0, 0)
    end

    if key_up("ESC") or GetKeyUp("ESC") then
        warn("[Sample] ESC released")
    end
end

function EndPlay()
    log("[Sample] EndPlay")

    if spawned_actor ~= nil and spawned_actor:IsValid() then
        spawned_actor:Destroy()
        log("[Sample] spawned actor destroyed from EndPlay")
    end
    log("[Sample] Complete Destroy")
end
