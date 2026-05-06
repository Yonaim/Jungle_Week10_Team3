local Table = {}

function Table.ShallowCopy(source)
    local copied = {}
    if type(source) ~= "table" then
        return copied
    end

    for key, value in pairs(source) do
        copied[key] = value
    end

    return copied
end

return Table
