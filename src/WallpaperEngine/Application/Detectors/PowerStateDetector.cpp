#include "PowerStateDetector.h"
#include "WallpaperEngine/Logging/Log.h"

#include <filesystem>
#include <fstream>
#include <string>

#ifdef ENABLE_DBUS
#include <dbus/dbus.h>
#endif

using namespace WallpaperEngine::Application::Detectors;

PowerStateDetector::PowerStateDetector (ApplicationContext& context)
    : m_context (context), m_lastCheckTime (std::chrono::steady_clock::now () - CHECK_INTERVAL) { }

bool PowerStateDetector::shouldPause () {
    if (!m_context.settings.render.pauseOnBattery && !m_context.settings.render.pauseOnPowerSaver) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now ();
    if (now - m_lastCheckTime < CHECK_INTERVAL) {
        return m_isPaused;
    }

    m_lastCheckTime = now;
    m_isPaused = false;

    if (m_context.settings.render.pauseOnPowerSaver && this->checkPowerSaver ()) {
        m_isPaused = true;
    } else if (m_context.settings.render.pauseOnBattery && this->checkBattery ()) {
        m_isPaused = true;
    }

    return m_isPaused;
}

bool PowerStateDetector::checkBattery () const {
#ifdef ENABLE_DBUS
    DBusError error;
    dbus_error_init (&error);

    DBusConnection* conn = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
    if (!conn) {
        dbus_error_free (&error);
        goto fallback;
    }

    {
        DBusMessage* msg = dbus_message_new_method_call (
            "org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "Get"
        );
        if (!msg) {
            goto fallback;
        }

        const char* interfaceStr = "org.freedesktop.UPower";
        const char* propertyStr = "OnBattery";

        dbus_message_append_args (
            msg, DBUS_TYPE_STRING, &interfaceStr, DBUS_TYPE_STRING, &propertyStr, DBUS_TYPE_INVALID
        );

        DBusMessage* reply = dbus_connection_send_with_reply_and_block (conn, msg, 1000, &error);
        dbus_message_unref (msg);

        if (!reply) {
            dbus_error_free (&error);
            goto fallback;
        }

        DBusMessageIter args;
        if (dbus_message_iter_init (reply, &args) && dbus_message_iter_get_arg_type (&args) == DBUS_TYPE_VARIANT) {
            DBusMessageIter variantArgs;
            dbus_message_iter_recurse (&args, &variantArgs);

            if (dbus_message_iter_get_arg_type (&variantArgs) == DBUS_TYPE_BOOLEAN) {
                dbus_bool_t onBattery;
                dbus_message_iter_get_basic (&variantArgs, &onBattery);
                dbus_message_unref (reply);
                return onBattery;
            }
        }
        dbus_message_unref (reply);
    }

fallback:
#endif

    // Fallback logic using sysfs
    const std::filesystem::path powerSupplyDir ("/sys/class/power_supply");
    if (std::filesystem::exists (powerSupplyDir) && std::filesystem::is_directory (powerSupplyDir)) {
        for (const auto& entry : std::filesystem::directory_iterator (powerSupplyDir)) {
            const auto typeFile = entry.path () / "type";
            if (std::filesystem::exists (typeFile)) {
                std::ifstream typeIn (typeFile);
                std::string typeStr;
                if (std::getline (typeIn, typeStr) && typeStr == "Battery") {
                    const auto statusFile = entry.path () / "status";
                    if (std::filesystem::exists (statusFile)) {
                        std::ifstream statusIn (statusFile);
                        std::string statusStr;
                        if (std::getline (statusIn, statusStr)) {
                            if (statusStr == "Discharging") {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool PowerStateDetector::checkPowerSaver () const {
#ifdef ENABLE_DBUS
    DBusError error;
    dbus_error_init (&error);

    DBusConnection* conn = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
    if (!conn) {
        dbus_error_free (&error);
        return false;
    }

    DBusMessage* msg = dbus_message_new_method_call (
        "net.hadess.PowerProfiles", "/net/hadess/PowerProfiles", "org.freedesktop.DBus.Properties", "Get"
    );
    if (!msg) {
        return false;
    }

    const char* interfaceStr = "net.hadess.PowerProfiles";
    const char* propertyStr = "ActiveProfile";

    dbus_message_append_args (msg, DBUS_TYPE_STRING, &interfaceStr, DBUS_TYPE_STRING, &propertyStr, DBUS_TYPE_INVALID);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block (conn, msg, 1000, &error);
    dbus_message_unref (msg);

    if (!reply) {
        dbus_error_free (&error);
        return false;
    }

    bool isPowerSaver = false;
    DBusMessageIter args;
    if (dbus_message_iter_init (reply, &args) && dbus_message_iter_get_arg_type (&args) == DBUS_TYPE_VARIANT) {
        DBusMessageIter variantArgs;
        dbus_message_iter_recurse (&args, &variantArgs);

        if (dbus_message_iter_get_arg_type (&variantArgs) == DBUS_TYPE_STRING) {
            char* profileName;
            dbus_message_iter_get_basic (&variantArgs, &profileName);
            if (std::string (profileName) == "power-saver") {
                isPowerSaver = true;
            }
        }
    }

    dbus_message_unref (reply);
    return isPowerSaver;
#else
    sLog.error ("Cannot check power profiles without dbus support");
    return false;
#endif
}
