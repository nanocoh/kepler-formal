#include <vector>
#include "BoolExpr.h"

namespace naja {
namespace NL {
class SNLTruthTable;
}
}  // namespace naja

namespace KEPLER_FORMAL {

class NajaClause {
 public:
  typedef std::vector<const naja::NL::SNLTruthTable&> InputsToMerge;

  NajaClause(InputsToMerge inputsToMerge,
                   const naja::NL::SNLTruthTable& base);

  naja::NL::SNLTruthTable mergeTruthTables(
    const naja::NL::SNLTruthTable &childTT,
    const std::vector<naja::NL::SNLTruthTable> &parents);

 private:
  const InputsToMerge& inputsToMerge_;
  const naja::NL::SNLTruthTable& base_;
  BoolExpr boolExpr_;
};

}  // namespace KEPLER_FORMAL