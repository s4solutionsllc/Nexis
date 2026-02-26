#include "health_score_calculator.h"

#include <QtGlobal>

HealthScoreCalculator::HealthScoreCalculator()
{
    mComponents = {
        {"cpu",     "CPU", 100, 0.15, true},
        {"memory",  "MEM", 100, 0.20, true},
        {"disk",    "DSK", 100, 0.25, true},
        {"temp",    "TMP", 100, 0.15, false},
        {"battery", "BAT", 100, 0.10, false},
        {"smart",   "HDD", 100, 0.15, false}
    };
}

void HealthScoreCalculator::setCpuScore(int score)
{
    int idx = indexOfComponent("cpu");
    if (idx >= 0)
        mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setMemoryScore(int score)
{
    int idx = indexOfComponent("memory");
    if (idx >= 0)
        mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setDiskScore(int score)
{
    int idx = indexOfComponent("disk");
    if (idx >= 0)
        mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setTempScore(int score)
{
    int idx = indexOfComponent("temp");
    if (idx >= 0)
        mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setBatteryScore(int score)
{
    int idx = indexOfComponent("battery");
    if (idx >= 0)
        mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setSmartScore(int score)
{
    int idx = indexOfComponent("smart");
    if (idx >= 0)
        mComponents[idx].score = qBound(0, score, 100);
}

void HealthScoreCalculator::setComponentAvailable(const QString &id, bool available)
{
    int idx = indexOfComponent(id);
    if (idx >= 0)
        mComponents[idx].available = available;
}

int HealthScoreCalculator::compositeScore() const
{
    double totalWeight = 0.0;
    double weightedSum = 0.0;

    for (const auto &c : mComponents) {
        if (!c.available)
            continue;
        totalWeight += c.weight;
        weightedSum += c.weight * c.score;
    }

    if (totalWeight <= 0.0)
        return 100;

    return qBound(0, static_cast<int>(qRound(weightedSum / totalWeight)), 100);
}

QString HealthScoreCalculator::scoreLabel() const
{
    int score = compositeScore();
    if (score >= 75) return QStringLiteral("Excellent");
    if (score >= 40) return QStringLiteral("Good");
    return QStringLiteral("Poor");
}

QList<HealthComponent> HealthScoreCalculator::components() const
{
    QList<HealthComponent> result;
    for (const auto &c : mComponents) {
        if (c.available)
            result.append(c);
    }
    return result;
}

int HealthScoreCalculator::indexOfComponent(const QString &id) const
{
    for (int i = 0; i < mComponents.size(); ++i) {
        if (mComponents[i].id == id)
            return i;
    }
    return -1;
}
