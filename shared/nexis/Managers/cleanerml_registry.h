#ifndef CLEANERML_REGISTRY_H
#define CLEANERML_REGISTRY_H

#include <Common/trust_safety_types.h>
#include <Managers/cleaner_action_interpreter.h>
#include <Tools/cleanerml_model.h>
#include <Tools/cleanerml_parser.h>

#include <QHash>
#include <QSet>
#include <memory>
#include <vector>

// SSO-25782: where the app's CleanerML cleaner definitions come from, and
// the provider that runs a selection of them through the Trust & Safety
// preview dialog.
namespace CleanerMLRegistry {

// The BleachBit community definitions compiled into the app.
QString bundledDir();
// Definitions the user adds; one with the same cleaner id replaces the
// bundled one.
QString userDir();

// Parses every directory leniently (later directories win on a duplicate
// id), reduces each cleaner to what runs on `os`, drops those with nothing
// left, and sorts by label.
QList<CleanerML::Cleaner> load(const QStringList &dirs, const QString &os,
                               QList<CleanerML::ParseError> *errors = nullptr);
QList<CleanerML::Cleaner> loadDefault(QList<CleanerML::ParseError> *errors = nullptr);

// True when at least one path the cleaner would act on exists, i.e. the
// application has left something on this machine.
bool hasData(const CleanerML::Cleaner &cleaner, const CleanerActionInterpreter::SandboxRoots &roots);

// True when one of the cleaner's <running type="exe"> processes exists.
bool isRunning(const CleanerML::Cleaner &cleaner);

// Runs several cleaners as one provider; each item is routed back to its
// cleaner by category id.
class BatchProvider : public TrustSafetyActionProvider
{
public:
    void add(const CleanerML::Cleaner &cleaner, const QSet<QString> &optionIds,
             const CleanerActionInterpreter::SandboxRoots &roots,
             const QList<CleanerService::ExclusionEntry> &exclusions);
    bool isEmpty() const { return mInterpreters.empty(); }

    void scan(QAtomicInt *cancelled,
              const std::function<void(const TrustSafetyActionItem &)> &itemFound) override;
    TrustSafetyActionResult performItem(const TrustSafetyActionItem &item, bool dryRun) override;

private:
    std::vector<std::unique_ptr<CleanerActionInterpreter>> mInterpreters;
    QHash<QString, CleanerActionInterpreter *> mByCleanerId;
};

}

#endif // CLEANERML_REGISTRY_H
