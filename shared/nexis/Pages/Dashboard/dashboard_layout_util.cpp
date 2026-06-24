#include "dashboard_layout_util.h"

#include <QJsonObject>
#include <QSet>
#include <algorithm>

namespace DashboardLayout {

DisplayTier tierForArea(int area)
{
    if (area >= 16) return Hero;
    if (area >= 8)  return Large;
    if (area >= 4)  return Normal;
    return Compact;
}

int columnsForWidth(int panelWidth)
{
    int pitch = kCellW + kGap;
    int cols = (panelWidth + kGap) / pitch;
    return std::clamp(cols, kMinCols, kMaxCols);
}

QJsonArray reflow(const QJsonArray &tiles, int cols)
{
    cols = std::max(1, cols);
    QSet<qint64> occ;
    auto key = [cols](int r, int c) { return static_cast<qint64>(r) * cols + c; };
    auto isFree = [&](int r, int c, int rs, int cs) {
        for (int rr = r; rr < r + rs; ++rr)
            for (int cc = c; cc < c + cs; ++cc)
                if (occ.contains(key(rr, cc))) return false;
        return true;
    };
    auto mark = [&](int r, int c, int rs, int cs) {
        for (int rr = r; rr < r + rs; ++rr)
            for (int cc = c; cc < c + cs; ++cc)
                occ.insert(key(rr, cc));
    };

    QJsonArray out;
    for (const QJsonValue &v : tiles) {
        QJsonObject o = v.toObject();
        int rs = std::max(1, o.value("rowSpan").toInt(1));
        int cs = std::clamp(o.value("colSpan").toInt(1), 1, cols);
        int pr = -1, pc = -1;
        for (int r = 0; pr < 0; ++r)                  // always terminates: an
            for (int c = 0; c <= cols - cs; ++c)      // empty row always fits
                if (isFree(r, c, rs, cs)) { pr = r; pc = c; break; }
        mark(pr, pc, rs, cs);
        o["row"] = pr; o["col"] = pc; o["rowSpan"] = rs; o["colSpan"] = cs;
        out.append(o);
    }
    return out;
}

QJsonArray migrate(const QJsonArray &tiles, int declaredVersion)
{
    if (declaredVersion >= kSchemaVersion)
        return tiles;

    constexpr int scale = kLegacyScale;             // was kGridCols / kLegacyGridCols

    QJsonArray out;
    for (const QJsonValue &val : tiles) {
        QJsonObject obj = val.toObject();

        int row = std::max(0, obj.value("row").toInt() * scale);
        int col = std::clamp(obj.value("col").toInt() * scale, 0, kMaxCols - 1);
        int rowSpan = std::max(1, obj.value("rowSpan").toInt(1) * scale);
        int colSpan = std::clamp(obj.value("colSpan").toInt(1) * scale, 1, kMaxCols - col);

        obj["row"] = row;
        obj["col"] = col;
        obj["rowSpan"] = rowSpan;
        obj["colSpan"] = colSpan;
        out.append(obj);
    }
    return out;
}

} // namespace DashboardLayout
