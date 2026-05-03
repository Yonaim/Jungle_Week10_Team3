-- PlayerSlide.lua
-- Runner 슬라이드 상태 기계.
--
-- PlayerController.lua는 입력, 이동, 라이프사이클만 담당하게 두고,
-- 슬라이드 중 바뀌는 collision shape / mesh transform은 이 모듈이 관리한다.

local PlayerSlide = {}
PlayerSlide.__index = PlayerSlide

local function is_valid_component(component)
    return component and component:IsValid()
end

local function find_collision_shape(actor)
    -- 현재 Runner는 UBoxComponent가 root collision이다.
    -- 그래도 shape 공통 API를 쓰도록 sphere/capsule까지 순서대로 찾는다.
    local shape = actor:GetComponentByType("UBoxComponent")
    if is_valid_component(shape) then
        return shape
    end

    shape = actor:GetComponentByType("USphereComponent")
    if is_valid_component(shape) then
        return shape
    end

    shape = actor:GetComponentByType("UCapsuleComponent")
    if is_valid_component(shape) then
        return shape
    end

    return nil
end

function PlayerSlide.new(actor)
    return setmetatable({
        actor = actor,
        is_sliding = false,

        collision_shape = nil,
        normal_shape_extent = nil,
        slide_shape_extent = nil,
        slide_shape_z_delta = 0.0,

        mesh = nil,
        normal_mesh_local_scale = nil,
        slide_mesh_local_scale = nil,
        normal_mesh_local_location = nil,
        slide_mesh_local_location = nil
    }, PlayerSlide)
end

function PlayerSlide:Configure()
    self.collision_shape = find_collision_shape(self.actor)
    if is_valid_component(self.collision_shape) then
        self.collision_shape:SetCollisionEnabled(true)
        self.collision_shape:SetGenerateOverlapEvents(true)

        self.normal_shape_extent = self.collision_shape:GetShapeExtent()
        if self.normal_shape_extent then
            self.slide_shape_extent = vec3(
                self.normal_shape_extent.x,
                self.normal_shape_extent.y,
                self.normal_shape_extent.z * 0.5
            )

            -- ShapeExtent.z는 box/capsule 계열에서 half-height로 본다.
            -- 슬라이드 때 줄어든 half-height만큼 아래로 내려야 바닥면이 그대로 유지된다.
            self.slide_shape_z_delta = self.normal_shape_extent.z - self.slide_shape_extent.z
        end

        print(
            "[TempleRun] Collision configured. Shape=" ..
            tostring(self.collision_shape:GetShapeType()) ..
            ", NormalExtent=(" ..
            tostring(self.normal_shape_extent and self.normal_shape_extent.x or 0.0) .. ", " ..
            tostring(self.normal_shape_extent and self.normal_shape_extent.y or 0.0) .. ", " ..
            tostring(self.normal_shape_extent and self.normal_shape_extent.z or 0.0) .. "), ShapeZDelta=" ..
            tostring(self.slide_shape_z_delta)
        )
    else
        warn("[TempleRun] Collision shape not found. Slide collision resize and ground height fallback may be inaccurate.")
    end

    self.mesh = self.actor:GetStaticMeshComponent()
    if is_valid_component(self.mesh) then
        self.normal_mesh_local_scale = self.mesh:GetLocalScale()
        self.normal_mesh_local_location = self.mesh:GetLocalLocation()

        self.slide_mesh_local_scale = vec3(
            self.normal_mesh_local_scale.x,
            self.normal_mesh_local_scale.y,
            self.normal_mesh_local_scale.z * 0.5
        )

        -- Static mesh scale은 pivot/center 기준으로 줄어든다.
        -- 그래서 시각 높이 감소분의 절반만큼 local Z를 내려야 mesh가 바닥에서 뜨지 않는다.
        self.slide_mesh_local_location = vec3(
            self.normal_mesh_local_location.x,
            self.normal_mesh_local_location.y,
            self.normal_mesh_local_location.z
        )

        print(
            "[TempleRun] Mesh slide configured. NormalScaleZ=" ..
            tostring(self.normal_mesh_local_scale.z) ..
            ", SlideScaleZ=" ..
            tostring(self.slide_mesh_local_scale.z)
        )
    else
        warn("[TempleRun] StaticMeshComponent not found. Slide visual scale will not work.")
    end
end

function PlayerSlide:GetCollisionShape()
    if is_valid_component(self.collision_shape) then
        return self.collision_shape
    end

    return nil
end

function PlayerSlide:IsSliding()
    return self.is_sliding
end

function PlayerSlide:Begin()
    if self.is_sliding then
        return
    end

    self.is_sliding = true

    if is_valid_component(self.collision_shape) and self.slide_shape_extent then
        self.collision_shape:SetShapeExtent(self.slide_shape_extent)

        -- Runner의 collision box는 현재 root component다.
        -- 저장해 둔 local location으로 되돌리면 달리던 actor의 X/Y까지 예전 위치로 튈 수 있다.
        -- 그래서 현재 위치 기준 AddLocalOffset만 사용한다.
        if self.slide_shape_z_delta ~= 0.0 then
            self.collision_shape:AddLocalOffset(vec3(0.0, 0.0, -self.slide_shape_z_delta))
        end

        print(
            "[TempleRun] Slide collision extent=(" ..
            tostring(self.slide_shape_extent.x) .. ", " ..
            tostring(self.slide_shape_extent.y) .. ", " ..
            tostring(self.slide_shape_extent.z) .. "), ShapeZDelta=" ..
            tostring(self.slide_shape_z_delta)
        )
    end

    if is_valid_component(self.mesh) then
        if self.slide_mesh_local_scale then
            self.mesh:SetLocalScale(self.slide_mesh_local_scale)
        end
        if self.slide_mesh_local_location then
            self.mesh:SetLocalLocation(self.slide_mesh_local_location)
        end
    end

    print("[TempleRun] Input: Slide")
end

function PlayerSlide:End()
    if not self.is_sliding then
        return
    end

    self:Restore()
    print("[TempleRun] Slide end")
end

function PlayerSlide:Restore()
    local was_sliding = self.is_sliding

    if is_valid_component(self.collision_shape) and self.normal_shape_extent then
        self.collision_shape:SetShapeExtent(self.normal_shape_extent)
        if was_sliding and self.slide_shape_z_delta ~= 0.0 then
            self.collision_shape:AddLocalOffset(vec3(0.0, 0.0, self.slide_shape_z_delta))
        end
    end

    if is_valid_component(self.mesh) then
        if self.normal_mesh_local_scale then
            self.mesh:SetLocalScale(self.normal_mesh_local_scale)
        end
        if self.normal_mesh_local_location then
            self.mesh:SetLocalLocation(self.normal_mesh_local_location)
        end
    end

    self.is_sliding = false
end

return PlayerSlide
