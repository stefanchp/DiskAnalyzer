#ifndef ANALYZER_H
#define ANALYZER_H

#include "tree.h"
typedef struct AnalysisJob AnalysisJob;

TreeNode *analyze_directory(const char *path, AnalysisJob *job);

#endif