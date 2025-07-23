#include <vector>
#include "BoolExpr.h"

#pragma once

namespace naja {
namespace NL {
class SNLDesign;
}
}  // namespace naja

namespace KEPLER_FORMAL {

class MiterStrategy {
 public:
  MiterStrategy(naja::NL::SNLDesign* top0, naja::NL::SNLDesign* top1)
      : top0_(top0), top1_(top1) {}

  bool run();

 private:
  std::shared_ptr<BoolExpr> buildMiter(
      const std::vector<std::shared_ptr<BoolExpr>>& A,
      const std::vector<std::shared_ptr<BoolExpr>>& B) const;

  naja::NL::SNLDesign* top0_ = nullptr;
  naja::NL::SNLDesign* top1_ = nullptr;
  std::vector<BoolExpr> POs0_;
  std::vector<BoolExpr> POs1_;
  BoolExpr miterClause_;
};

}  // namespace KEPLER_FORMAL