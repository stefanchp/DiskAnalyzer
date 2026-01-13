#ifndef ANALYZER_H
#define ANALYZER_H

#include "tree.h"
struct AnalysisJob;

TreeNode *analyze_directory(const char *path, struct AnalysisJob *job);

#endif