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
    PsiSnapshot getCpuSnapshot() const { return mCpu; }

private:
    static PsiSnapshot parseFile(const char *path);

    PsiSnapshot mCpu;
};

#endif // PSI_INFO_H
