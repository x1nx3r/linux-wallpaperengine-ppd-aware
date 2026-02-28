#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include <chrono>

namespace WallpaperEngine::Application::Detectors {
/**
 * Detects the current power state of the system, including whether it is on battery
 * or in power-saver mode.
 */
class PowerStateDetector {
public:
    explicit PowerStateDetector (ApplicationContext& context);

    /**
     * @return True if the system is in a state where rendering should be paused according to settings
     */
    [[nodiscard]] bool shouldPause ();

private:
    ApplicationContext& m_context;
    std::chrono::steady_clock::time_point m_lastCheckTime;

    bool m_isPaused = false;

    /**
     * Minimum time between checking the power state to avoid spamming dbus/sysfs
     */
    static constexpr auto CHECK_INTERVAL = std::chrono::seconds (2);

    /**
     * Checks if the system is currently discharging battery
     */
    [[nodiscard]] bool checkBattery () const;

    /**
     * Checks if the system is currently in power-saver mode
     */
    [[nodiscard]] bool checkPowerSaver () const;
};
} // namespace WallpaperEngine::Application::Detectors
