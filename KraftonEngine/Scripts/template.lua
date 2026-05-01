-- Lua Script Template
-- Lua는 실제 Actor를 직접 소유하지 않는다.
-- obj, spawn_actor, find_actor가 돌려주는 값은 모두 C++ Actor를 약하게 참조하는 ActorProxy이다.

EnemyAI = {}

function EnemyAI.start()
    -- 이 함수는 start_coroutine으로 실행되는 코루틴이다.
    -- wait 계열 함수는 Lua 실행 전체를 멈추지 않고 C++ Coroutine Scheduler에 대기 조건만 넘긴다.
    log("[AI] Start")

    obj:SetMoveSpeed(300)

    for i = 1, 3 do
        -- MoveTo는 방향 벡터가 아니라 월드 좌표 목표 지점으로 이동을 시작한다.
        obj:MoveTo(100 * i, 0)

        -- 이동이 끝날 때까지 이 코루틴만 대기한다.
        -- 게임 전체 Tick과 다른 Actor의 스크립트는 계속 진행된다.
        wait_until_move_done()

        -- 현재는 실제 애니메이션 시스템 대신 C++ mock 애니메이션을 사용한다.
        obj:PlayAnimation("Patrol")
        wait_anim_done("Patrol")

        -- wait는 반드시 코루틴 안에서만 호출한다.
        -- 일반 Tick()에서 직접 wait()를 호출하면 lifecycle 함수가 yield되어 에러가 된다.
        wait(1.0)
    end

    log("[AI] Done")
end

function SpawnWave()
    -- spawn_actor는 Actor를 생성하지만 소유권은 World/Object system에 남는다.
    -- Lua는 반환된 ActorProxy를 통해 허용된 기능만 호출한다.
    for i = 1, 5 do
        local enemy = spawn_actor("AStaticMeshActor", vec3(i * 100, 0, 0))

        if enemy:IsValid() then
            enemy:SetMoveSpeed(200)

            -- MoveToActor는 raw AActor*가 아니라 ActorProxy를 받는다.
            -- C++ 쪽에서 매 Tick 대상 Actor 생존 여부를 다시 확인한다.
            enemy:MoveToActor(obj)
        end

        -- wait 계열 함수는 코루틴 안에서만 사용한다.
        wait(0.5)
    end
end

function WaitSignalTest()
    log("waiting signal")

    -- wait_signal은 signal("PlayerDetected")가 호출되는 프레임까지 이 코루틴만 대기한다.
    wait_signal("PlayerDetected")
    log("signal received")
end

function BeginPlay()
    -- BeginPlay/Tick/EndPlay 같은 일반 lifecycle 함수에서는 wait를 직접 호출하지 않는다.
    -- 대기 흐름이 필요하면 start_coroutine으로 별도 코루틴을 시작한다.
    start_coroutine("EnemyAI.start")
    -- start_coroutine("SpawnWave")
    -- start_coroutine("WaitSignalTest")
end

function Tick(dt)
    -- key_down은 GetKeyDown의 Lua 스타일 alias이다.
    -- signal은 한 프레임짜리 이벤트라서 같은 프레임의 coroutine scheduler가 소비한 뒤 지워진다.
    if key_down("SPACE") then
        signal("PlayerDetected")
    end
end
