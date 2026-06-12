#ifndef BATTERY_CHARGE_THRESHOLD_H
#define BATTERY_CHARGE_THRESHOLD_H

// FW-15 (SSO-3743): Linux-only battery charge-threshold control.
// Reads and writes charge_control_end_threshold (and optionally
// charge_control_start_threshold) via pkexec. Gated on the sysfs
// node existing. Writes are verified by read-back.
//
// All public methods are static so callers need no instance.
// readStatus() accepts an optional path override for unit testing.

#include <QString>

struct ChargeThresholdStatus {
    bool    available    = false;  // charge_control_end_threshold node exists
    bool    hasStart     = false;  // charge_control_start_threshold also exists
    int     endPct       = -1;    // current end threshold (%)
    int     startPct     = -1;    // current start threshold (%)
    QString batteryPath;           // e.g. /sys/class/power_supply/BAT0
    QString batteryName;           // e.g. BAT0
    QString errorMsg;
};

struct ChargeThresholdResult {
    bool    ok           = false;
    int     verifiedEnd  = -1;
    int     verifiedStart = -1;
    QString errorMsg;
};

class BatteryChargeThreshold
{
public:
    static constexpr int kPresetMaximize     = 100;
    static constexpr int kPresetPreserve     = 80;
    static constexpr int kPresetPreserveStart = 75;
    static constexpr int kMinEndThreshold    = 50;

    // Discover battery and read current thresholds.
    // Pass a non-empty overridePath (pointing at a BAT* dir) to skip
    // discovery — used by unit tests against fixture directories.
    static ChargeThresholdStatus readStatus(const QString &overridePath = QString());

    // Write thresholds to the given batteryPath via pkexec + tee.
    // startPct == -1 means skip start_threshold (hardware may lack it).
    // Performs read-back verify; returns verifiedEnd/verifiedStart.
    static ChargeThresholdResult writeThreshold(const QString &batteryPath,
                                                 bool hasStart,
                                                 int  endPct,
                                                 int  startPct = -1);

    // Pure validation — returns empty string on success, error message otherwise.
    static QString validateThreshold(int endPct, int startPct = -1);

    // Returns a udev rules file body that re-applies the threshold on boot.
    // batteryName is e.g. "BAT0".
    static QString buildUdevRule(const QString &batteryName, int endPct, int startPct = -1);

    // udev rules file path used for persistence.
    static constexpr const char *kUdevRulesPath =
        "/etc/udev/rules.d/99-nexis-battery-threshold.rules";

private:
    static QString findBatteryPath();
};

#endif // BATTERY_CHARGE_THRESHOLD_H
