#ifndef PSI_INFO_H
#define PSI_INFO_H

struct PsiSnapshot {
    bool   available     = false;
    double someAvg10     = 0.0;
    double someAvg60     = 0.0;
    double someAvg300    = 0.0;
    double fullAvg10     = 0.0;
    double fullAvg60     = 0.0;
    double fullAvg300    = 0.0;
};

class PsiInfo
{
public:
    void updateCpuPsi();
    void updateMemoryPsi();
    void updateIoPsi();
    PsiSnapshot getCpuSnapshot() const { return mCpu; }
    PsiSnapshot getMemorySnapshot() const { return mMemory; }
    PsiSnapshot getIoSnapshot() const { return mIo; }

    // Public (FR-76-style) for testability — parses any /proc/pressure/*
    // file; a missing file returns a default-constructed (unavailable)
    // snapshot rather than throwing, since SSO-26721 wants a missing memory
    // or io pressure file to just hide that line, not the card.
    static PsiSnapshot parsePressureFile(const char *path);

private:
    PsiSnapshot mCpu;
    PsiSnapshot mMemory;
    PsiSnapshot mIo;
};

#endif // PSI_INFO_H
