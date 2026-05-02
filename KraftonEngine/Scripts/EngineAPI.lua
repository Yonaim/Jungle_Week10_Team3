---@meta KraftonEngine

-- 이 파일은 VSCode/LuaLS 자동완성 전용 타입 stub입니다.
-- 런타임에서 require하지 않습니다. 실제 구현은 C++ LuaScriptRuntime/LuaScriptInstance 바인딩에 있습니다.

---@class Vector
---@field x number
---@field y number
---@field z number
---@operator add(Vector): Vector
---@operator sub(Vector): Vector
---@operator mul(number): Vector
---@operator div(number): Vector
local Vector = {}

---@param x number
---@param y number
---@param z number
---@return Vector
function Vector.new(x, y, z) end

---@class Actor
---@field Name string
---@field UUID integer
---@field Location Vector
---@field Rotation Vector
---@field Scale Vector
---@field Velocity Vector
local Actor = {}

---@return boolean
function Actor:IsValid() end

---@param delta Vector
function Actor:AddWorldOffset(delta) end

---@param x number
---@param y number
---@param z number
function Actor:AddWorldOffset(x, y, z) end

---@param target Vector
function Actor:MoveTo(target) end

---@param x number
---@param y number
---@param z? number
function Actor:MoveTo(x, y, z) end

---@param delta Vector
function Actor:MoveBy(delta) end

---@param x number
---@param y number
---@param z? number
function Actor:MoveBy(x, y, z) end

---@param target Actor
function Actor:MoveToActor(target) end

function Actor:StopMove() end

---@return boolean
function Actor:IsMoveDone() end

---@param speed number
function Actor:SetMoveSpeed(speed) end

---@return number
function Actor:GetMoveSpeed() end

function Actor:PrintLocation() end

---@type Actor
obj = nil

---@param x number
---@param y number
---@param z number
---@return Vector
function vec3(x, y, z) end

---@param ... any
function log(...) end

---@param ... any
function warn(...) end

---@param ... any
function error_log(...) end

---@param ... any
function print(...) end

---@return number
function time() end

---@return number
function delta_time() end

---@param keyName string
---@return boolean
function key(keyName) end

---@param keyName string
---@return boolean
function key_down(keyName) end

---@param keyName string
---@return boolean
function key_up(keyName) end

---@param functionName string
---@return boolean
function StartCoroutine(functionName) end

---@param seconds number
function wait(seconds) end

---@param frames integer
function wait_frames(frames) end

---@alias ScriptPropertyType '"bool"'|'"int"'|'"float"'|'"string"'|'"vector"'

---@class ScriptPropertyDesc
---@field type ScriptPropertyType
---@field default? boolean|integer|number|string|Vector
---@field min? number
---@field max? number

---@param properties table<string, ScriptPropertyDesc>
function DeclareProperties(properties) end

---@generic T
---@param name string
---@param defaultValue T
---@return T
function property(name, defaultValue) end
