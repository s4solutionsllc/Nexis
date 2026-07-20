#ifndef PROCESS_H
#define PROCESS_H

#include "Utils/file_util.h"

#include "nexis-core_global.h"

class NEXISCORESHARED_EXPORT Process {

public:
    pid_t getPid() const;
    void setPid(const pid_t &value);

    quint64 getRss() const;
    void setRss(const quint64 &value);

    double getPmem() const;
    void setPmem(const double &value);

    quint64 getVsize() const;
    void setVsize(const quint64 &value);

    QString getUname() const;
    void setUname(const QString &value);

    double getPcpu() const;
    void setPcpu(const double &value);

    QString getStartTime() const;
    void setStartTime(const QString &value);

    QString getState() const;
    void setState(const QString &value);

    QString getGroup() const;
    void setGroup(const QString &value);

    int getNice() const;
    void setNice(const int &value);

    QString getCpuTime() const;
    void setCpuTime(const QString &value);

    QString getSession() const;
    void setSession(const QString &value);

    double getDiskReadRate() const;
    void setDiskReadRate(const double &value);

    double getDiskWriteRate() const;
    void setDiskWriteRate(const double &value);

    double getNetDownRate() const;
    void setNetDownRate(const double &value);

    double getNetUpRate() const;
    void setNetUpRate(const double &value);

    // FR-115: per-process GPU utilization (%) and VRAM (bytes).
    // Sentinel -1 means "unknown" — the view renders an em-dash.
    double getGpuPercent() const;
    void setGpuPercent(const double &value);

    qint64 getGpuVramBytes() const;
    void setGpuVramBytes(const qint64 &value);

    QString getCmd() const;
    void setCmd(const QString &value);

    // GH#194: short process name (Linux: /proc/<pid>/comm), distinct from the
    // full command line in getCmd(). Empty on platforms that don't collect it.
    QString getName() const;
    void setName(const QString &value);

    // SSO-15376: App vs Background classification — Linux: pid belongs to a
    // systemd app-*.scope (desktop session) cgroup; macOS: binary path is
    // inside a .app bundle's Contents/MacOS. Background is the default, so
    // an unclassifiable process never falls through un-grouped.
    bool getIsAppProcess() const;
    void setIsAppProcess(bool value);

    // SSO-15376: platform icon-resolution hint for App-classified processes —
    // Linux: an XDG .desktop Icon= value (theme name or absolute path);
    // macOS: the .app bundle path. Empty when unresolved; the GUI falls back
    // to a generic icon rather than leaving the row blank.
    QString getIconHint() const;
    void setIconHint(const QString &value);

private:
    pid_t pid;
    quint64 rss;
    double pmem;
    quint64 vsize;
    QString uname;
    double pcpu;
    QString startTime;
    QString state;
    QString group;
    int nice;
    QString cpuTime;
    QString session;
    double diskReadRate = -1.0;
    double diskWriteRate = -1.0;
    double netDownRate = -1.0;
    double netUpRate = -1.0;
    double gpuPercent = -1.0;
    qint64 gpuVramBytes = -1;
    QString cmd;
    QString name;
    bool isAppProcess = false;
    QString iconHint;
};


#endif // PROCESS_H
