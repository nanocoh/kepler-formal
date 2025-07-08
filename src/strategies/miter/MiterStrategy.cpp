#include "MiterStrategy.h"
#include "BoolExpr.h"
#include "DNL.h"
#include "SNLDesignModeling.h"

using namespace KEPLER_FORMAL;
using namespace naja::DNL;
using namespace naja::NL;

void MiterStrategy::collectInputs() {
  std::vector<DNLID> inputs;
  auto dnl = get();
  // Add top ports
  DNLInstanceFull top = dnl->getTop();
  for (DNLID termId = top.getTermIndexes().first;
       termId != DNLID_MAX and termId <= top.getTermIndexes().second;
       termId++) {
    const DNLTerminalFull& term = dnl->getDNLTerminalFromID(termId);
    if (term.getSnlBitTerm()->getDirection() != SNLBitTerm::Direction::Output) {
      inputs.push_back(termId);
    }
  }
  // Add Sequential outputs
  for (DNLID leaf : dnl->getLeaves()) {
    DNLInstanceFull instance = dnl->getDNLInstanceFromID(leaf);
    for (DNLID termId = top.getTermIndexes().first;
         termId != DNLID_MAX and termId <= top.getTermIndexes().second;
         termId++) {
      const DNLTerminalFull& term = dnl->getDNLTerminalFromID(termId);
      if (term.getSnlBitTerm()->getDirection() !=
          SNLBitTerm::Direction::Input) {
        if (SNLDesignModeling::getClockRelatedOutputs(term.getSnlBitTerm())
                .size() > 0) {
          inputs.push_back(termId);
        }
      }
    }
  }
}

void MiterStrategy::build() {
  // Collect inputs
}
