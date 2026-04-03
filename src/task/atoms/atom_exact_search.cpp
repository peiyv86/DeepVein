#include "atom_exact_search.h"

QList<DocChunk> AtomExactSearch::execute(const QString& targetFileName) {
    if (targetFileName.isEmpty()) return {};
    return Datamanager::getInstance().searchByFileName(targetFileName);
}
