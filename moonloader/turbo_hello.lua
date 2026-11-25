-- Sample Lua script showcasing OpenMoonLoader Turbo v2.0.0
script_name("TurboHello")
script_version("2.0.0")

function main()
    local ver = getOpenMoonVersion and getOpenMoonVersion() or "standalone"
    local bootUs = getStartupMicroseconds and getStartupMicroseconds() or 0
    print(string.format("[%s] Booted in %.2f ms", ver, bootUs / 1000.0))
end
