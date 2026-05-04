-- PlayerController.lua
-- Runner Pawn의 입력, 자동 전진, 레인 이동, 중력, 바닥 판정, Stability/GameOver 연결을 담당한다.
-- C++ Runner는 최소한의 Actor/Component 구성만 만들고, 실제 플레이 수치와 상태 전이는 Lua에서 조정한다.
-- Map과 Player 충돌 문제를 추적하기 쉽도록 Ground Query 관련 값과 상태 변화는 이벤트 단위로 로그를 남긴다.

local Config = require("Game.Config")
local GameManager = require("Game.GameManager")
local PlayerStatus = require("Game.PlayerStatus")
local AudioManager = require("Game.AudioManager")
local PlayerSlide = require("Game.PlayerSlide")

local PlayerConfig = Config.player

DeclareProperties({
    { name = "ForwardSpeed", type = "float", default = PlayerConfig.forward_speed },
    { name = "LaneWidth", type = "float", default = PlayerConfig.lane_width },
    { name = "LaneChangeSpeed", type = "float", default = PlayerConfig.lane_change_speed },
    { name = "Gravity", type = "float", default = PlayerConfig.gravity },
    { name = "JumpPower", type = "float", default = PlayerConfig.jump_power },
})

-- =========================================================
-- 런타임 상태
-- =========================================================

-- forward_speed는 Runner가 매 프레임 자동으로 앞으로 가는 속도입니다.
local forward_speed = property("ForwardSpeed", PlayerConfig.forward_speed)
-- lane_width는 레인 사이 간격입니다.
local lane_width = property("LaneWidth", PlayerConfig.lane_width)
-- lane_change_speed는 목표 레인으로 부드럽게 이동하는 속도입니다.
local lane_change_speed = property("LaneChangeSpeed", PlayerConfig.lane_change_speed)
-- gravity는 공중에 있을 때 적용되는 수직 가속도입니다.
local gravity = property("Gravity", PlayerConfig.gravity)
-- jump_power는 점프 시작 순간 위로 주는 속도입니다.
local jump_power = property("JumpPower", PlayerConfig.jump_power)
-- max_move_step은 빠른 전진 중 overlap 누락을 줄이기 위해 이동을 쪼개는 최대 거리입니다.
local max_move_step = PlayerConfig.max_move_step or 0.25

-- current_lane은 현재 도착한 레인 번호입니다.
local current_lane = 0
-- target_lane은 입력으로 지정된 목표 레인 번호입니다.
local target_lane = 0

-- vertical_velocity는 점프/낙하에 쓰는 현재 Z 속도입니다.
local vertical_velocity = 0.0
-- is_grounded는 이번 프레임 기준 바닥에 붙어 있는지 나타냅니다.
local is_grounded = false
-- current_ground_z는 마지막으로 찾은 바닥 상단 Z입니다.
local current_ground_z = nil
-- last_ground_hit는 바닥 발견/상실 로그를 한 번만 찍기 위한 이전 상태입니다.
local last_ground_hit = false

-- ground_probe_distance는 발 아래로 바닥을 얼마나 멀리까지 찾을지입니다.
local ground_probe_distance = PlayerConfig.ground_probe_distance
-- ground_snap_distance는 거의 닿았을 때만 바닥에 붙이는 허용 거리입니다.
local ground_snap_distance = PlayerConfig.ground_snap_distance
-- skin_width는 바닥/충돌 판정 여유값입니다.
local skin_width = PlayerConfig.skin_width
-- fallback_half_height는 collision shape를 못 찾았을 때 쓰는 반높이입니다.
local fallback_half_height = PlayerConfig.fallback_half_height
-- fall_dead_z는 이 높이 아래로 떨어지면 낙사 처리하는 기준입니다.
local fall_dead_z = PlayerConfig.fall_dead_z

-- initial_location은 시작 위치 기록용입니다. 나중에 리스폰/디버그에서 쓰기 좋게 남깁니다.
local initial_location = nil
-- slide는 PlayerSlide 모듈 인스턴스입니다. collision/mesh 변경은 여기로 위임합니다.
local slide = nil
-- game_over_tick_logged는 GameOver 후 Tick 중단 로그를 한 번만 찍기 위한 플래그입니다.
local game_over_tick_logged = false
-- half_height_fallback_logged는 collision half-height fallback 로그를 한 번만 찍기 위한 플래그입니다.
local half_height_fallback_logged = false
-- previous_key_state는 A/D/Space가 꾹 눌린 동안 매 프레임 반복 발동하지 않게 이전 프레임 상태를 저장합니다.
local previous_key_state = {}
-- current_key_state는 이번 프레임에 확인한 키 상태를 임시로 모아두는 테이블입니다.
local current_key_state = {}

-- =========================================================
-- 공통 유틸리티
-- =========================================================

local function log(message)
    -- log는 PlayerController 흐름 확인용 디버그 출력입니다.
    if Config.debug.enable_log then
        print(message)
    end
end

local function clamp(value, min_value, max_value)
    -- clamp는 레인 번호처럼 범위가 정해진 값을 안전하게 자릅니다.
    if value < min_value then
        return min_value
    end
    if value > max_value then
        return max_value
    end
    return value
end

local function abs(value)
    -- abs는 Lua math.abs 대신 짧게 쓰는 절댓값 helper입니다.
    if value < 0.0 then
        return -value
    end
    return value
end

local function sign(value)
    -- sign은 레인 보간 방향을 -1/0/1로 바꿉니다.
    if value < 0.0 then
        return -1.0
    end
    if value > 0.0 then
        return 1.0
    end
    return 0.0
end

local function round(value)
    -- round는 시작 위치 Y를 가장 가까운 레인 번호로 바꿀 때 씁니다.
    if value >= 0.0 then
        return math.floor(value + 0.5)
    end
    return math.ceil(value - 0.5)
end

local function copy_vec(v)
    -- copy_vec는 엔진 vec3 값을 새 vec3로 복사해 시작 위치를 저장합니다.
    return vec3(v.x, v.y, v.z)
end

local function lane_min()
    -- lane_min은 가장 왼쪽 레인 번호입니다.
    return -math.floor(PlayerConfig.lane_count / 2)
end

local function lane_max()
    -- lane_max는 가장 오른쪽 레인 번호입니다.
    return math.floor(PlayerConfig.lane_count / 2)
end

local function key_held(name)
    -- key_held는 "지금 키를 누르고 있는가"만 확인합니다.
    -- 슬라이드는 조작감 때문에 누르고 있는 동안 유지해야 하므로 edge trigger가 아니라 held 상태를 씁니다.
    if GetKey then
        return GetKey(name)
    end
    return false
end

local function key_pressed_once(name)
    -- InputSystem의 GetKeyDown이 환경에 따라 held처럼 들어와도 안전하게 막기 위해 Lua에서 edge trigger를 직접 만듭니다.
    -- A/D 레인 이동과 Space 점프는 "방금 누른 순간"에만 처리하면 됨.
    local is_down = key_held(name)
    local was_down = previous_key_state[name] == true
    current_key_state[name] = is_down
    return is_down and not was_down
end

local function finish_input_frame()
    -- 이번 프레임에 확인한 키 상태를 다음 프레임 previous로 넘깁니다.
    -- 확인하지 않은 키는 false로 내려서 다음 입력이 다시 edge로 잡히게 합니다.
    for key, _ in pairs(previous_key_state) do
        if current_key_state[key] == nil then
            current_key_state[key] = false
        end
    end

    previous_key_state = current_key_state
    current_key_state = {}
end

-- =========================================================
-- 바닥 판정 헬퍼
-- =========================================================

local function get_player_half_height()
    -- Player의 발 위치는 "Actor 중심 Z - collision half-height"로 계산된다.
    -- FindGround()도 이 half-height를 기준으로 현재 발 아래에 있는 floor AABB를 찾는다.
    -- 따라서 collision shape를 못 찾으면 fallback 값을 써야 하지만,
    -- fallback이 너무 작거나 크면 바닥이 위/아래로 잘못 판정될 수 있으므로 최초 1회 로그를 남긴다.
    local collision_shape = slide and slide:GetCollisionShape() or nil
    if collision_shape and collision_shape.GetShapeHalfHeight then
        local half_height = collision_shape:GetShapeHalfHeight()
        if half_height then
            return half_height
        end
    end

    if not half_height_fallback_logged then
        log("[PlayerController] Box half height fallback=" .. tostring(fallback_half_height))
        half_height_fallback_logged = true
    end
    return fallback_half_height
end

local function update_ground_state(dt, allow_snap)
    -- Map floor는 StaticMeshComponent의 world AABB로 판정된다.
    -- ground_probe_distance는 "발 아래로 얼마나 멀리까지 바닥을 찾을지"이고,
    -- skin_width는 X/Y 수평 겹침 판정에 약간의 여유를 주는 값이다.
    -- C++ 쪽 floor collision이 꺼져 있거나 bounds가 갱신되지 않으면 여기서 ground.hit이 false가 된다.
    local loc = obj:GetWorldLocation()
    local half_height = get_player_half_height()
    local ground = obj:FindGround(ground_probe_distance, skin_width)

    if ground and ground.hit then
        if not last_ground_hit then
            log(
                "[PlayerController] Ground found ground_z=" .. tostring(ground.ground_z) ..
                " distance=" .. tostring(ground.distance)
            )
        end
        last_ground_hit = true
        current_ground_z = ground.ground_z

        local desired_center_z = current_ground_z + half_height
        local distance_to_ground = loc.z - desired_center_z

        -- snap은 떨어지는 중이거나 정지 상태일 때만 수행한다.
        -- 점프 상승 중에 위쪽 floor나 obstacle 상단으로 강제로 붙으면 조작감이 깨지므로
        -- vertical_velocity <= 0 조건을 유지한다.
        -- Ground snap은 바닥보다 아주 살짝 아래로 파고든 정도까지만 허용합니다.
        -- distance_to_ground가 너무 큰 음수면 플레이어가 지면 아래/이상한 위치에 있는 것이므로 잘못 끌어올리지 않습니다.
        if allow_snap and vertical_velocity <= 0.0
            and distance_to_ground >= -skin_width
            and distance_to_ground <= ground_snap_distance then
            loc.z = desired_center_z
            obj:SetWorldLocation(loc)
            vertical_velocity = 0.0
            is_grounded = true
            return
        end
    else
        if last_ground_hit then
            log("[PlayerController] Ground lost")
        end
        last_ground_hit = false
        current_ground_z = nil
    end

    is_grounded = false
end

-- =========================================================
-- 레인 이동 / 점프 입력
-- =========================================================

local function move_lane(delta)
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return
    end

    local previous_lane = target_lane
    target_lane = clamp(target_lane + delta, lane_min(), lane_max())

    if previous_lane ~= target_lane then
        log(
            "[PlayerController] Lane change input delta=" .. tostring(delta) ..
            " target_lane=" .. tostring(target_lane) ..
            " target_y=" .. tostring(target_lane * lane_width)
        )
    end
end

local function try_jump()
    if PlayerStatus.IsDead() or GameManager.IsGameOver() or not is_grounded then
        return
    end

    vertical_velocity = jump_power
    is_grounded = false
    AudioManager.PlayJump()
    log("[PlayerController] Jump velocity=" .. tostring(jump_power))
end

-- =========================================================
-- 중력 / 낙하 사망
-- =========================================================

local function check_fall_death()
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return true
    end

    local loc = obj:GetWorldLocation()
    if loc.z < fall_dead_z then
        log(
            "[PlayerController] Fall death z=" .. tostring(loc.z) ..
            " threshold=" .. tostring(fall_dead_z)
        )
        PlayerStatus.Kill("Fall")
        return true
    end

    return false
end

local function apply_gravity(dt)
    -- 이동 전 먼저 현재 발 아래 바닥을 검사한다.
    -- 이미 floor 위에 있으면 vertical_velocity를 0으로 만들고, 불필요한 낙하 누적을 막는다.
    update_ground_state(dt, true)

    if not is_grounded then
        vertical_velocity = vertical_velocity + gravity * dt

        local loc = obj:GetWorldLocation()
        loc.z = loc.z + vertical_velocity * dt
        obj:SetWorldLocation(loc)

        -- 중력으로 내려온 뒤 다시 검사한다.
        -- 이 두 번째 검사가 있어야 한 프레임 안에서 floor를 통과하지 않고 바로 착지/snap할 수 있다.
        update_ground_state(dt, true)
    end
end

-- =========================================================
-- 슬라이드 입력
-- =========================================================

local function slide_key_pressed()
    -- 슬라이드는 duration 기반 타이머가 아니라 누르고 있는 동안 유지하는 방식입니다.
    -- S/아래/CTRL 중 하나를 잡고 있으면 slide 유지, 모두 떼면 종료합니다.
    return key_held("S") or key_held("DOWN") or key_held("Down") or key_held("CTRL") or key_held("Control")
end

local function begin_slide()
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return
    end

    if not is_grounded then
        -- 공중에서 slide Begin을 걸면 점프/낙하 상태와 충돌할 수 있으므로 지상에서만 시작합니다.
        return
    end

    if slide and slide:IsSliding() then
        -- 슬라이드 중 Begin이 반복 호출되면 timer/shape가 계속 초기화되던 문제를 막습니다.
        return
    end

    if slide then
        slide:Begin()
        AudioManager.PlaySlide()
        log("[PlayerController] Slide start hold")
    end
end

local function update_slide(dt, wants_slide)
    if not slide or not slide:IsSliding() then
        return
    end

    -- 슬라이드는 Config.player에 남은 타이머 값이 아니라 현재 입력 상태와 지상 여부로 끝냅니다.
    -- 키를 떼거나 공중으로 뜨면 안전하게 slide를 끝냅니다.
    if not wants_slide or not is_grounded then
        slide:End()
        log("[PlayerController] Slide finished by key release or air")
    end
end

-- =========================================================
-- 장애물 충돌 콜백 헬퍼
-- =========================================================

local function is_valid_actor(actor)
    return actor and actor.IsValid and actor:IsValid()
end

local function has_obstacle_tag(actor)
    if not is_valid_actor(actor) then
        return false
    end

    if actor:HasTag("Obstacle") then
        return true
    end

    if actor.Name and string.find(tostring(actor.Name), "Obstacle") then
        actor.Tag = "Obstacle"
        log("[PlayerController] Obstacle tag fallback applied actor=" .. tostring(actor.Name))
        return true
    end

    return false
end

local function handle_obstacle_collision(event_name, other_actor)
    if PlayerStatus.IsDead() or GameManager.IsGameOver() then
        return
    end

    if not is_valid_actor(other_actor) then
        return
    end

    log("[PlayerController] " .. event_name .. " actor=" .. tostring(other_actor.Name))

    if not has_obstacle_tag(other_actor) then
        return
    end

    local damage = Config.obstacle.default_damage
    -- 장애물별 피해량을 다르게 줄 수 있게 C++ Damage 값을 Lua에서 읽는다.
    -- 바인딩 안 된 장애물은 Config.obstacle.default_damage를 사용합니다.
    if other_actor and other_actor.GetDamage then
        local obstacle_damage = other_actor:GetDamage()
        if obstacle_damage and obstacle_damage > 0 then
            damage = obstacle_damage
        end
    end
    log("[PlayerController] Obstacle collision damage=" .. tostring(damage))
    PlayerStatus.DamageStability(damage)
end

-- =========================================================
-- 생명주기
-- =========================================================

function BeginPlay()
    -- Runner C++ 생성자에서 시작 위치를 바닥보다 약간 높은 Z로 둔다.
    -- MapManager가 Player BeginPlay 이후에 Chunk를 만들 수 있으므로,
    -- 첫 ground query가 실패하더라도 다음 Tick에서 생성된 floor를 찾고 중력으로 snap되게 한다.
    local loc = obj:GetWorldLocation()
    initial_location = copy_vec(loc)

    current_lane = clamp(round(loc.y / lane_width), lane_min(), lane_max())
    target_lane = current_lane

    vertical_velocity = 0.0
    is_grounded = false
    current_ground_z = nil
    last_ground_hit = false
    game_over_tick_logged = false
    half_height_fallback_logged = false
    previous_key_state = {}
    current_key_state = {}

    obj.Tag = "Player"

    slide = PlayerSlide.new(obj)
    slide:Configure()

    log("[PlayerController] BeginPlay")
    log(
        "[PlayerController] Config forward_speed=" .. tostring(forward_speed) ..
        " max_move_step=" .. tostring(max_move_step) ..
        " lane_width=" .. tostring(lane_width) ..
        " lane_count=" .. tostring(PlayerConfig.lane_count) ..
        " gravity=" .. tostring(gravity) ..
        " jump_power=" .. tostring(jump_power) ..
        " slide_mode=hold" ..
        " fall_dead_z=" .. tostring(fall_dead_z)
    )
    log(
        "[PlayerController] Ground config probe=" .. tostring(ground_probe_distance) ..
        " snap=" .. tostring(ground_snap_distance) ..
        " skin=" .. tostring(skin_width) ..
        " fallback_half_height=" .. tostring(fallback_half_height)
    )

    local audio_ok = AudioManager.Initialize(obj)
    log("[PlayerController] AudioManager.Initialize result=" .. tostring(audio_ok))

    GameManager.StartGame()
    log("[PlayerController] GameManager.StartGame")

    PlayerStatus.ResetForStart()
    log("[PlayerController] PlayerStatus.ResetForStart")

    update_ground_state(0.0, true)
end

function Tick(dt)
    -- GameOver 이후에는 이동, 입력, 중력, score 갱신을 모두 멈춘다.
    -- 로그는 최초 1회만 남겨 Tick마다 콘솔이 도배되지 않게 한다.
    if GameManager.IsGameOver() then
        if not game_over_tick_logged then
            log("[PlayerController] Tick stopped: GameOver")
            game_over_tick_logged = true
            if slide then
                slide:Restore()
            end
        end
        return
    end

    PlayerStatus.Tick(dt)
    if PlayerStatus.IsDead() then
        return
    end

    if check_fall_death() then
        return
    end

    local move_left_pressed = key_pressed_once("A") or key_pressed_once("LEFT") or key_pressed_once("Left")
    local move_right_pressed = key_pressed_once("D") or key_pressed_once("RIGHT") or key_pressed_once("Right")
    local jump_pressed = key_pressed_once("SPACE") or key_pressed_once("Space")
    local wants_slide = slide_key_pressed()
    finish_input_frame()

    if move_left_pressed then
        move_lane(-1)
    elseif move_right_pressed then
        move_lane(1)
    end

    if jump_pressed then
        try_jump()
    end

    if wants_slide then
        begin_slide()
    end
    update_slide(dt, wants_slide)

    -- score는 "실제로 전진시킨 거리"를 GameManager에 넘겨서 계산한다.
    -- 고속 이동 시 한 프레임 이동 거리를 잘게 나눠 teleport 방식 overlap 누락을 줄인다.
    local total_distance = forward_speed * dt
    local safe_max_step = max_move_step
    if safe_max_step == nil or safe_max_step <= 0.0 then
        safe_max_step = 0.25
    end

    local steps = math.max(1, math.ceil(math.abs(total_distance) / safe_max_step))
    local step_dt = dt / steps
    local step_distance = total_distance / steps
    local actual_moved_distance = 0.0

    for _ = 1, steps do
        if PlayerStatus.IsDead() or GameManager.IsGameOver() then
            break
        end

        local loc = obj:GetWorldLocation()
        loc.x = loc.x + step_distance
        actual_moved_distance = actual_moved_distance + step_distance

        local target_y = target_lane * lane_width
        local diff_y = target_y - loc.y
        if abs(diff_y) > 0.1 then
            local step = lane_change_speed * step_dt
            if abs(diff_y) <= step then
                loc.y = target_y
                current_lane = target_lane
                log("[PlayerController] Lane reached lane=" .. tostring(current_lane) .. " y=" .. tostring(loc.y))
            else
                loc.y = loc.y + sign(diff_y) * step
            end
        else
            loc.y = target_y
            current_lane = target_lane
        end

        obj:SetWorldLocation(loc)
        apply_gravity(step_dt)

        if check_fall_death() then
            break
        end
    end

    GameManager.Tick(dt, actual_moved_distance)
end

function EndPlay()
    if slide then
        slide:Restore()
    end
    log("[PlayerController] EndPlay")
end

-- =========================================================
-- 충돌 콜백
-- =========================================================

function OnBeginOverlap(otherActor, otherComp, selfComp)
    handle_obstacle_collision("BeginOverlap", otherActor)
end

function OnEndOverlap(otherActor, otherComp, selfComp)
    if not is_valid_actor(otherActor) then
        return
    end

    log("[PlayerController] EndOverlap actor=" .. tostring(otherActor.Name))
end

function OnHit(otherActor, otherComp, selfComp, impactLocation, impactNormal)
    handle_obstacle_collision("Hit", otherActor)
end
