#ifndef NEXIS_ROLES_H
#define NEXIS_ROLES_H

#include <Qt>

/// Custom data roles used throughout Nexis UI tables and trees.
/// Using named constants instead of magic numbers makes intent clear
/// and catches misuse at compile time.
enum NexisDataRole {
    SortRole       = Qt::UserRole + 1,  // replaces bare `1` and `0x0100`
    LineNumberRole = Qt::UserRole + 2   // replaces bare `9` in host_manage
};

#endif // NEXIS_ROLES_H
