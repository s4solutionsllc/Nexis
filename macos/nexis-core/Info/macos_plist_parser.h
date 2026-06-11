#ifndef MACOS_PLIST_PARSER_H
#define MACOS_PLIST_PARSER_H

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVariant>

// WI-33: pure parser for the `<dict>`-shaped XML plists that diskutil emits
// (`diskutil list -plist`, `diskutil info -plist`). Extracted from
// disk_health_info.cpp so it can be exercised by fixture tests without
// shelling out to diskutil.
//
// Handles <string>, <integer>, <true/>, <false/>, <array> of strings, and a
// single level of nested <dict>. The nested SMARTDeviceSpecificKeysMayVary…
// dict gets its keys prefixed with "SMART." to keep a flat result map.
namespace MacosPlistParser {

QMap<QString, QVariant> parse(const QByteArray &data);

} // namespace MacosPlistParser

#endif // MACOS_PLIST_PARSER_H
