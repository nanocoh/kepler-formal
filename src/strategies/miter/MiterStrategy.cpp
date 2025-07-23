#include "MiterStrategy.h"
#include "BuildPrimaryOutputClauses.h"
#include "SNLDesignModeling.h"
#include "SNLTruthTable2BoolExpr.h"
#include "SNLDesignTruthTable.h"
#include "SNLLogicCloud.h"
#include "NLUniverse.h"
#include "SimpleSatSolver.h"
#include "BoolExpr.h"

using namespace KEPLER_FORMAL;

bool MiterStrategy::run() {
  BuildPrimaryOutputClauses builder;

  SNLDesign* topInit = NLUniverse::get()->getTopDesign();
  NLUniverse* univ = NLUniverse::get();
  univ->setTopDesign(top0_);
  builder.build();
  auto POs0 = builder.getPOs();
  univ->setTopDesign(top1_);
  builder.build();
  auto POs1 = builder.getPOs();
  univ->setTopDesign(topInit);  // Restore top design

  if (POs0.empty() || POs1.empty()) {
    // No outputs to compare, miter is always false
    return false;
  }

  // Build miter clause
  miterClause_ = *buildMiter(POs0, POs1);

  // Check if miter is satisfiable
  SimplSatSolver solver({miterClause_});
  std::unordered_map<std::string, bool> assignment;
  if (!solver.solve(assignment)) {
    // Miter is unsatisfiable, meaning outputs are equivalent
    //miterClause_ = BoolExpr::createTrue();
    return true;
  }

  return false;
}

std::shared_ptr<BoolExpr> MiterStrategy ::buildMiter(
    const std::vector<std::shared_ptr<BoolExpr>>& A,
    const std::vector<std::shared_ptr<BoolExpr>>& B) const {
  if (A.size() != B.size()) {
    printf("Miter inputs must match in length: %zu vs %zu\n", A.size(), B.size());
  }
  assert(A.size() == B.size() && "Miter inputs must match in length");

  // Empty miter = always‐false (no outputs to compare)
  if (A.empty())
    return BoolExpr::createFalse();

  // Start with the first XOR
  auto miter = BoolExpr::Xor(A[0], B[0]);

  // OR in the rest
  for (size_t i = 1, n = A.size(); i < n; ++i) {
    auto diff = BoolExpr::Xor(A[i], B[i]);
    miter = BoolExpr::Or(miter, diff);
  }
  return miter;
}