#ifndef HEALTH_SCORE_CALCULATOR_H
#define HEALTH_SCORE_CALCULATOR_H

#include <QList>
#include <QPair>
#include <QString>

struct HealthComponent {
    QString id;       // "cpu", "memory", "disk", "temp", "battery", "smart"
    QString label;    // "CPU", "MEM", "DSK", "TMP", "BAT", "HDD"
    int score;        // 0-100
    double weight;    // default weight (before redistribution)
    bool available;   // false if hardware not present
};

class HealthScoreCalculator
{
public:
    HealthScoreCalculator();

    void setCpuScore(int score);
    void setMemoryScore(int score);
    void setDiskScore(int score);
    void setTempScore(int score);
    void setBatteryScore(int score);
    void setSmartScore(int score);

    void setComponentAvailable(const QString &id, bool available);

    int compositeScore() const;
    QString scoreLabel() const;          // "Excellent", "Good", "Fair", "Poor"
    QList<HealthComponent> components() const;

private:
    QList<HealthComponent> mComponents;
    int indexOfComponent(const QString &id) const;
};

#endif // HEALTH_SCORE_CALCULATOR_H
