#include "util/gitinfostore.h"

#define GIT_INFO
#include "gitinfo.h"

namespace GitInfoStore {

QString branch() {
    return QStringLiteral(GIT_BRANCH);
}

QString describe() {
    return QStringLiteral(GIT_DESCRIBE);
}

QString date() {
    return QStringLiteral(GIT_COMMIT_DATE);
}

int commitCount() {
#ifdef GIT_COMMIT_COUNT
    return GIT_COMMIT_COUNT;
#else
    return 0;
#endif
}

bool dirty() {
#ifdef GIT_DIRTY
    return true;
#else
    return false;
#endif
}

} // namespace GitInfoStore
