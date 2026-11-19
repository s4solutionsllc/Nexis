#ifndef AI_MODELS_DIALOG_H
#define AI_MODELS_DIALOG_H

#include <QDialog>
#include <QList>

#include <Tools/ai_model_store_tool.h>

class QLabel;
class QPushButton;
class QTreeWidget;

// SSO-27099: lists the local AI models downloaded by Ollama, the Hugging Face
// hub cache, LM Studio and llama.cpp, one row per model, and removes the ones
// the user ticks. Nothing is pre-selected, and nothing here is reachable from
// a scheduled or automatic clean.
class AiModelsDialog : public QDialog
{
    Q_OBJECT

public:
    AiModelsDialog(const QList<AiModelStore::Model> &models,
                   const AiModelStore::Environment &env,
                   QWidget *parent = nullptr);

    QList<AiModelStore::Model> selectedModels() const;

    // The per-row heads-up: what a later re-download costs and how the owning
    // tool's own list can go stale. Shown in the row and in the confirmation.
    static QString warningFor(const AiModelStore::Model &model, bool usesToolCommand);

    // "3 selected · frees 8.2 GB". Shared data counts once, and only when every
    // model using it is ticked.
    static QString summaryFor(const QList<AiModelStore::Model> &selection);

private slots:
    void updateSelection();
    void onRemoveClicked();

private:
    void populate();

    AiModelStore::Environment mEnv;
    QList<AiModelStore::Model> mModels;
    QTreeWidget *mTree;
    QLabel *mLblSummary;
    QPushButton *mBtnRemove;
};

#endif // AI_MODELS_DIALOG_H
