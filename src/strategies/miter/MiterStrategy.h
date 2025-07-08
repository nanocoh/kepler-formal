#include <vector>

namespace KEPLER_FORMAL {
class BoolExpr;

class MiterStrategy {
 public:
  MiterStrategy() = default;

  void build();

  void collectInputs();

 private:
  std::vector<BoolExpr> POs_;
};

}  // namespace KEPLER_FORMAL