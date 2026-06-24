#include "dashboard_layout_util.h"

#include <QJsonObject>
#include <algorithm>

namespace DashboardLayout {

DisplayTier tierForArea(int area)
{
    if (area >= 16) return Hero;
    if (area >= 8)  return Large;
    if (area >= 4)  return Normal;
    return Compact;
}

QJsonArray migrate(const QJsonArray &tiles, int declaredVersion)
{
    if (declaredVersion >= kSchemaVersion)
        return tiles;

    constexpr int scale = kGridCols / kLegacyGridCols; // 2

    QJsonArray out;
    for (const QJsonValue &val : tiles) {
        QJsonObject obj = val.toObject();

        int row = std::clamp(obj.value("row").toInt() * scale, 0, kGridRows - 1);
        int col = std::clamp(obj.value("col").toInt() * scale, 0, kGridCols - 1);
        int rowSpan = std::clamp(obj.value("rowSpan").toInt(1) * scale, 1, kGridRows - row);
        int colSpan = std::clamp(obj.value("colSpan").toInt(1) * scale, 1, kGridCols - col);

        obj["row"] = row;
        obj["col"] = col;
        obj["rowSpan"] = rowSpan;
        obj["colSpan"] = colSpan;
        out.append(obj);
    }
    return out;
}

} // namespace DashboardLayout
