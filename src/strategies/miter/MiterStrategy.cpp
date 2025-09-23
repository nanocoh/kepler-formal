// MiterStrategyWithGlucose.cpp

#include "MiterStrategy.h"
#include "BoolExpr.h"
#include "BuildPrimaryOutputClauses.h"
#include "NLUniverse.h"
#include "SNLDesignModeling.h"
#include "SNLDesignModeling.h"
#include "SNLLogicCloud.h"
#include "SNLTruthTable2BoolExpr.h"

// include Glucose headers (adjust path to your checkout)
#include "core/Solver.h"
#include "simp/SimpSolver.h"

#include <string>
#include <unordered_map>
#include "NetlistGraph.h"
#include "SNLEquipotential.h"
#include "SNLLogicCone.h"
#include "SNLPath.h"

// For executeCommand
#include <cstdlib>

using namespace naja;
using namespace naja::NL;
using namespace KEPLER_FORMAL;

namespace {

void executeCommand(const std::string& command) {
  int result = system(command.c_str());
  if (result != 0) {
    std::cerr << "Command execution failed." << std::endl;
  }
}
//
// A tiny Tseitin‐translator from BoolExpr→Glucose CNF.
//
// Returns a Glucose::Lit that stands for `e`, and adds
// all necessary clauses to S so that Lit ↔ (e) holds.
//
// node2var      caches each subformula’s fresh variable index.
// varName2idx   coalesces all inputs of the same name to one var.
//
Glucose::Lit tseitinEncode(Glucose::SimpSolver& S,
                           const std::shared_ptr<BoolExpr>& e,
                           std::unordered_map<const BoolExpr*, int>& node2var,
                           std::unordered_map<std::string, int>& varName2idx) {
  auto getOrCreateVar = [&](const std::string& key) -> int {
    auto it = varName2idx.find(key);
    if (it != varName2idx.end()) return it->second;
    int v = S.newVar();
    varName2idx[key] = v;
    return v;
  };

  auto constLit = [&](bool value) -> Glucose::Lit {
    const std::string key = value ? "$__CONST_TRUE__" : "$__CONST_FALSE__";
    int v = getOrCreateVar(key);
    Glucose::Lit lv = Glucose::mkLit(v);
    // Duplicating a unit clause is harmless; if you want, you can guard with a set.
    if (value) S.addClause(lv);
    else       S.addClause(~lv);
    return lv;
  };

  // Reuse already-encoded node
  if (auto it = node2var.find(e.get()); it != node2var.end())
    return Glucose::mkLit(it->second);

  // Optional: if your BoolExpr has explicit constant API, prefer it:
  // if (e->isConst()) {
  //   Glucose::Lit lc = constLit(e->constValue()); // bool
  //   node2var[e.get()] = Glucose::var(lc);
  //   return lc;
  // }

  // Handle leaf variables (and constant-like names)
  if (e->getOp() == Op::VAR) {
    const std::string& name = e->getName();

    // Treat common constant spellings as real constants
    if (name == "0" || name == "false" || name == "False" || name == "FALSE") {
      Glucose::Lit lf = constLit(false);
      node2var[e.get()] = Glucose::var(lf);
      return lf;
    }
    if (name == "1" || name == "true" || name == "True" || name == "TRUE") {
      Glucose::Lit lt = constLit(true);
      node2var[e.get()] = Glucose::var(lt);
      return lt;
    }

    // Regular named variable
    int v = getOrCreateVar(name);
    node2var[e.get()] = v;
    return Glucose::mkLit(v);
  }

  // Tseitin variable for this gate
  int v = S.newVar();
  Glucose::Lit lit_v = Glucose::mkLit(v);
  node2var[e.get()] = v;

  auto encodeChild = [&](const std::shared_ptr<BoolExpr>& c) {
    return tseitinEncode(S, c, node2var, varName2idx);
  };

  switch (e->getOp()) {
    case Op::NOT: {
      auto a = encodeChild(e->getLeft());
      // v ↔ ¬a  ≡ (¬v ∨ ¬a) ∧ (v ∨ a)
      S.addClause(~lit_v, ~a);
      S.addClause( lit_v,  a);
      break;
    }
    case Op::AND: {
      auto a = encodeChild(e->getLeft());
      auto b = encodeChild(e->getRight());
      // v ↔ (a ∧ b) ≡ (¬v ∨ a) ∧ (¬v ∨ b) ∧ (v ∨ ¬a ∨ ¬b)
      S.addClause(~lit_v,  a);
      S.addClause(~lit_v,  b);
      S.addClause( lit_v, ~a, ~b);
      break;
    }
    case Op::OR: {
      auto a = encodeChild(e->getLeft());
      auto b = encodeChild(e->getRight());
      // v ↔ (a ∨ b) ≡ (¬a ∨ v) ∧ (¬b ∨ v) ∧ (¬v ∨ a ∨ b)
      S.addClause(~a,  lit_v);
      S.addClause(~b,  lit_v);
      S.addClause(~lit_v, a, b);
      break;
    }
    case Op::XOR: {
      auto a = encodeChild(e->getLeft());
      auto b = encodeChild(e->getRight());
      // v ↔ (a ⊕ b)
      S.addClause(~lit_v, ~a, ~b);
      S.addClause(~lit_v,  a,  b);
      S.addClause( lit_v, ~a,  b);
      S.addClause( lit_v,  a, ~b);
      break;
    }
    default:
      // Extend with other ops as needed
      break;
  }

  return lit_v;
}
}  // namespace

void MiterStrategy::normalizeInputs(std::vector<naja::DNL::DNLID>& inputs0,
                       std::vector<naja::DNL::DNLID>& inputs1) {
    // find the intersection of inputs0 and inputs1 based on the getFullPathIDs of DNLTerminal and the diffs
    std::unordered_map<naja::DNL::DNLID, std::vector<NLID::DesignObjectID>> inputs0Map;
    for (const auto& input0 : inputs0) {
      inputs0Map[input0] = DNL::get()->getDNLTerminalFromID(input0).getFullPathIDs();
    }
    std::unordered_map<naja::DNL::DNLID, std::vector<NLID::DesignObjectID>> inputs1Map;
    for (const auto& input1 : inputs1) {
      inputs1Map[input1] = DNL::get()->getDNLTerminalFromID(input1).getFullPathIDs();
    }
    std::vector<naja::DNL::DNLID> commonInputs;
    for (const auto& [input0, path0] : inputs0Map) {
      for (const auto& [input1, path1] : inputs1Map) {
        if (path0 == path1) {
          commonInputs.push_back(input0);
          break;
        }
      }
    }
    std::vector<naja::DNL::DNLID> diff0;
    for (const auto& input0 : inputs0) {
      if (std::find(commonInputs.begin(), commonInputs.end(), input0) == commonInputs.end()) {
        diff0.push_back(input0);
      }
    }
    std::vector<naja::DNL::DNLID> diff1;
    for (const auto& input1 : inputs1) {
      if (std::find(commonInputs.begin(), commonInputs.end(), input1) == commonInputs.end()) {
        diff1.push_back(input1);
      }
    }
    inputs0 = commonInputs;
    inputs0.insert(inputs0.end(), diff0.begin(), diff0.end());
    inputs1 = commonInputs;
    inputs1.insert(inputs1.end(), diff1.begin(), diff1.end());
}

void MiterStrategy::normalizeOutputs(std::vector<naja::DNL::DNLID>& outputs0,
                        std::vector<naja::DNL::DNLID>& outputs1) {
    // find the intersection of outputs0 and outputs1 based on the getFullPathIDs of DNLTerminal and the diffs
    std::unordered_map<naja::DNL::DNLID, std::vector<NLID::DesignObjectID>> outputs0Map;
    for (const auto& output0 : outputs0) {
      outputs0Map[output0] = DNL::get()->getDNLTerminalFromID(output0).getFullPathIDs();
    }
    std::unordered_map<naja::DNL::DNLID, std::vector<NLID::DesignObjectID>> outputs1Map;
    for (const auto& output1 : outputs1) {
      outputs1Map[output1] = DNL::get()->getDNLTerminalFromID(output1).getFullPathIDs();
    }
    std::vector<naja::DNL::DNLID> commonOutputs;
    for (const auto& [output0, path0] : outputs0Map) {
      for (const auto& [output1, path1] : outputs1Map) {
        if (path0 == path1) {
          commonOutputs.push_back(output0);
          break;
        }
      }
    }
    std::vector<naja::DNL::DNLID> diff0;
    for (const auto& output0 : outputs0) {
      if (std::find(commonOutputs.begin(), commonOutputs.end(), output0) == commonOutputs.end()) {
        diff0.push_back(output0);
      }
    }
    std::vector<naja::DNL::DNLID> diff1;
    for (const auto& output1 : outputs1) {
      if (std::find(commonOutputs.begin(), commonOutputs.end(), output1) == commonOutputs.end()) {
        diff1.push_back(output1);
      }
    }
    outputs0 = commonOutputs;
    outputs0.insert(outputs0.end(), diff0.begin(), diff0.end());
    outputs1 = commonOutputs;
    outputs1.insert(outputs1.end(), diff1.begin(), diff1.end());
}

bool MiterStrategy::run() {
  // build both sets of POs
  topInit_ = NLUniverse::get()->getTopDesign();
  NLUniverse* univ = NLUniverse::get();
  naja::DNL::destroy();
  univ->setTopDesign(top0_);
  BuildPrimaryOutputClauses builder0;
  builder0.collect();
  builder0.build();
  const auto& PIs0 = builder0.getInputs();
  const auto& POs0 = builder0.getPOs();
  auto outputs0 = builder0.getOutputs();
  auto inputs2inputsIDs0 = builder0.getInputs2InputsIDs();
  auto outputs2outputsIDs0 = builder0.getOutputs2OutputsIDs();
  naja::DNL::destroy();
  univ->setTopDesign(top1_);
  BuildPrimaryOutputClauses builder1;
  builder1.collect();
  builder1.build();
  const auto& PIs1 = builder1.getInputs();
  const auto& POs1 = builder1.getPOs();
  auto outputs1 = builder1.getOutputs();
  auto inputs2inputsIDs1 = builder1.getInputs2InputsIDs();
  auto outputs2outputsIDs1 = builder1.getOutputs2OutputsIDs();

  std::vector<naja::DNL::DNLID> outputs2DnlIds = builder1.getOutputs();

  if (topInit_ != nullptr) {
    univ->setTopDesign(topInit_);
  }

  if (POs0.empty() || POs1.empty()) {
    return false;
  }
  if (outputs0 != outputs1) {
    for (size_t i = 0; i < outputs0.size(); ++i) {
      printf("Output %zu: %lu vs %lu\n", i, outputs0[i], outputs1[i]);
    }
    assert(outputs0 == outputs1);
  }
  // print POs0 and POs1 in 2 separate for loops
  for (size_t i = 0; i < POs0.size(); ++i) {
    printf("POs0[%zu]: %s\n", i, POs0[i]->toString().c_str());
  }
  for (size_t i = 0; i < POs1.size(); ++i) {
    printf("POs1[%zu]: %s\n", i, POs1[i]->toString().c_str());
  }
  

  // Check if both sets inputs are the same
  if (inputs2inputsIDs0.size() != inputs2inputsIDs1.size() ||
      outputs2outputsIDs0.size() != outputs2outputsIDs1.size()) {
    printf("Miter inputs/outputs must match in size: %zu vs %zu\n",
           inputs2inputsIDs0.size(), inputs2inputsIDs1.size());
    return false;
  }

  for (const auto& [id0, ids0] : inputs2inputsIDs0) {
    auto it = inputs2inputsIDs1.find(id0);
    if (it == inputs2inputsIDs1.end() || it->second.first != ids0.first ||
        it->second.second != ids0.second) {
      printf("Miter inputs must match in IDs: %lu\n", id0);
      return false;
    }
  }

  // Check if both sets outputs are the same
  if (outputs2outputsIDs0.size() != outputs2outputsIDs1.size()) {
    printf("Miter outputs must match in size: %zu vs %zu\n",
           outputs2outputsIDs0.size(), outputs2outputsIDs1.size());
    return false;
  }

  for (const auto& [id0, ids0] : outputs2outputsIDs0) {
    auto it = outputs2outputsIDs1.find(id0);
    if (it == outputs2outputsIDs1.end() || it->second.first != ids0.first ||
        it->second.second != ids0.second) {
      printf("Miter outputs must match in IDs: %lu\n", id0);
      return false;
    }
  }

  // build the Boolean‐miter expression
  auto miter = buildMiter(POs0, POs1);

  // Now SAT check via Glucose
  Glucose::SimpSolver solver;

  // mappings for Tseitin encoding
  std::unordered_map<const BoolExpr*, int> node2var;
  std::unordered_map<std::string, int> varName2idx;

  // Tseitin‐encode & get the literal for the root
  Glucose::Lit rootLit = tseitinEncode(solver, miter, node2var, varName2idx);

  // Assert root == true
  solver.addClause(rootLit);

  // solve with no assumptions
  printf("Started Glucose solving\n");
  bool sat = solver.solve();
  printf("Finished Glucose solving: %s\n", sat ? "SAT" : "UNSAT");

  if (sat) {
    for (size_t i = 0; i < POs0.size(); ++i) {
      tbb::concurrent_vector<std::shared_ptr<BoolExpr>> singlePOs0S;
      singlePOs0S.push_back(POs0[i]);
      tbb::concurrent_vector<std::shared_ptr<BoolExpr>> singlePOs1S;
      singlePOs1S.push_back(POs1[i]);
      auto singleMiter = buildMiter(singlePOs0S, singlePOs1S);
      
      std::unordered_map<const BoolExpr*, int> singleNode2var;
      std::unordered_map<std::string, int> singleVarName2idx;
      // Tseitin‐encode the single miter
      Glucose::SimpSolver singleSolver;
      Glucose::Lit singleRootLit = tseitinEncode(
          singleSolver, singleMiter, singleNode2var, singleVarName2idx);
      // print singleMiter
      printf("Single miter for PO %zu: %s\n", i,
           singleMiter->toString().c_str());
      

      singleSolver.addClause(singleRootLit);
      if (singleSolver.solve()) {
        // print singleMitter
        printf("------ PO failed for %zu mitter: %s\n", i,
             singleMiter->toString().c_str());


        printf("------ PO failed for %zu: %s vs %s\n", i,
             singlePOs0S[0]->toString().c_str(),
             singlePOs1S[0]->toString().c_str());
        failedPOs_.push_back(outputs0[i]);
        // If UNSAT, print the single miter
        printf("Check failed for PO: %zu\n", i);
        std::vector<naja::NL::SNLDesign*> topModels;
        topModels.push_back(top0_);
        topModels.push_back(top1_);
        std::vector<std::vector<naja::DNL::DNLID>> PIs;
        PIs.push_back(PIs0);
        PIs.push_back(PIs1);
        for (size_t j = 0; j < topModels.size(); ++j) {
          DNL::destroy();
          NLUniverse::get()->setTopDesign(topModels[j]);
          SNLLogicCone cone(outputs0[i], PIs[j]);
          cone.run();
          std::string dotFileNameEquis(
              std::string(prefix_ + "_" + DNL::get()->getDNLTerminalFromID(outputs0[i]).getSnlBitTerm()->getName().getString() + "_" +std::to_string(j) + std::string(".dot")));
          std::string svgFileNameEquis(
              std::string(prefix_ + "_" + DNL::get()->getDNLTerminalFromID(outputs0[i]).getSnlBitTerm()->getName().getString() + "_" + std::to_string(j) + std::string(".svg")));
          SnlVisualiser snl2(topModels[j], cone.getEquipotentials());
          snl2.process();
          snl2.getNetlistGraph().dumpDotFile(dotFileNameEquis.c_str());
          executeCommand(std::string(std::string("dot -Tsvg ") +
                                     dotFileNameEquis + std::string(" -o ") +
                                     svgFileNameEquis)
                             .c_str());
          printf("svg file name: %s\n", svgFileNameEquis.c_str());
        }
      }
    }
  }
  if (topInit_ != nullptr) {
    univ->setTopDesign(topInit_);
  }
  // if UNSAT → miter can never be true → outputs identical
  printf("Circuits are %s\n", sat ? "DIFFERENT" : "IDENTICAL");
  return !sat;
}

std::shared_ptr<BoolExpr> MiterStrategy::buildMiter(
    const tbb::concurrent_vector<std::shared_ptr<BoolExpr>>& A,
    const tbb::concurrent_vector<std::shared_ptr<BoolExpr>>& B) const {
  if (A.size() != B.size()) {
    printf("Miter inputs must match in length: %zu vs %zu\n", A.size(),
           B.size());
    assert(false && "Miter inputs must match in length");
  }

  // Empty miter = always‐false (no outputs to compare)
  if (A.empty()) {
    assert(false);
    return BoolExpr::createFalse();
  }
  printf("A[0]: %s\n", A[0]->toString().c_str());
  printf("B[0]: %s\n", B[0]->toString().c_str());
  // Start with the first XOR
  auto miter = BoolExpr::Xor(A[0], B[0]);
  printf("Initial miter: %s\n", miter->toString().c_str());
  // OR in the rest
  for (size_t i = 1, n = A.size(); i < n; ++i) {
    auto diff = BoolExpr::Xor(A[i], B[i]);
    miter = BoolExpr::Or(miter, diff);
  }
  return miter;
}
