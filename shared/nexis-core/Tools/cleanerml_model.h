#ifndef CLEANERML_MODEL_H
#define CLEANERML_MODEL_H

#include <QString>
#include <QStringList>
#include <QList>

#include "nexis-core_global.h"

// Internal, in-memory representation of a CleanerML (BleachBit-compatible)
// cleaner definition. Produced by CleanerMLParser; deliberately holds no
// execution logic — path expansion ($$var$$ substitution, glob/regex
// matching, actually deleting/truncating/vacuuming) is a separate concern
// left to a future execution engine (SSO-15366 epic).

namespace CleanerML {

// The subset of CleanerML <action command="..." search="..."/> combinations
// Nexis understands. winreg.* actions are recognized by the parser (so a
// cleaner isn't rejected just for containing one) but never appear here —
// they're filtered out before an Option's actions list is built, since
// Linux/macOS builds have no Windows registry to act on.
enum class ActionType {
    Delete,        // command="delete", search="file" (or omitted): remove one path
    Glob,          // command="delete", search="glob": remove paths matching a glob
    Walk,          // command="delete", search="walk.all"/"walk.files" (no regex): remove a directory tree
    Regex,         // command="delete", search="walk.files"+regex= or search="deep": remove paths matching a filename/path regex
    Truncate,      // command="truncate": zero out a file instead of removing it
    SqliteVacuum,  // command="sqlite.vacuum": VACUUM a sqlite database file
    Winreg,        // command="winreg"/"winreg.*": recognized, but the parser always
                   // filters these out of Option::actions before returning — Linux/macOS
                   // builds have no registry, so this value never reaches a caller.
};

struct NEXISCORESHARED_EXPORT Action {
    ActionType type = ActionType::Delete;
    QString path;    // raw path as written in the XML; $$var$$ tokens are left intact
    QString regex;   // populated for ActionType::Regex from the regex= or wholeregex= attribute
    QString command; // raw command= attribute, kept for diagnostics/logging
    QString search;  // raw search= attribute (may be empty), kept for diagnostics/logging
};

struct NEXISCORESHARED_EXPORT Option {
    QString id;
    QString label;
    QString description;
    QString warning; // optional <warning> text; empty when the option carries none
    QList<Action> actions;
};

struct NEXISCORESHARED_EXPORT Cleaner {
    QString id;
    QString label;
    QString description;
    QString warning;    // optional cleaner-level <warning> text
    QStringList os;      // parsed from os="linux,windows,macos"; empty means "all platforms"
    QList<Option> options;
};

} // namespace CleanerML

#endif // CLEANERML_MODEL_H
