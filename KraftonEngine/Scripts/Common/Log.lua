local Log = {}

function Log.IsEnabled(config)
    if config == nil then
        return false
    end

    if config.enable_log ~= nil then
        return config.enable_log == true
    end

    return config.debug ~= nil and config.debug.enable_log == true
end

function Log.Write(config, message, prefix)
    if Log.IsEnabled(config) then
        print((prefix or "") .. tostring(message))
    end
end

function Log.MakeLogger(config, prefix)
    return function(message)
        Log.Write(config, message, prefix)
    end
end

return Log
