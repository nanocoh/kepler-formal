#include "SNLTruthTable.h"
#include "NajaClause.h"
#include <vector>
#include <stdexcept>
#include <cstdint>

using namespace naja::NL;

/// Merge a “child” truth‐table with a set of “parent” truth‐tables,
/// where each parent’s single‐bit output drives exactly one input of the child.
///
/// - childTT.size() must equal parents.size().
/// - Each parentTT has its own number of inputs.
/// - The resulting TT has input count = sum(parentTT[i].size()).
/// - If the new input‐count ≤ 6, we pack into a uint64_t mask; otherwise
///   we build a vector<bool> mask.
SNLTruthTable mergeTruthTables(
    const SNLTruthTable &childTT,
    const std::vector<SNLTruthTable> &parents)
{
  // 1) Sanity check
  uint32_t K = uint32_t(parents.size());
  if (childTT.size() != K)
    throw std::invalid_argument(
      "mergeTruthTables: child arity != number of parents");

  // 2) Compute total new inputs = Σ parent.inputs
  uint32_t newSize = 0;
  for (auto &p : parents) newSize += p.size();

  uint32_t Nrows = 1u << newSize;

  // 3) Build the raw bit‐vector of length 2^newSize
  std::vector<bool> raw(Nrows);
  for (uint32_t idx = 0; idx < Nrows; ++idx) {
    bool childArgs[64];      // up to 64 parents supported
    uint32_t bitOff = 0;

    // slice each parent’s bits, eval its TT
    for (uint32_t i = 0; i < K; ++i) {
      uint32_t pSize = parents[i].size();
      uint32_t pIdx  = 0;
      for (uint32_t b = 0; b < pSize; ++b) {
        bool bv = ((idx >> (bitOff + b)) & 1) != 0;
        pIdx |= (uint32_t(bv) << b);
      }
      childArgs[i] = parents[i].bits().bit(pIdx);
      bitOff += pSize;
    }

    // pack childArgs into childIdx
    uint32_t childIdx = 0;
    for (uint32_t i = 0; i < K; ++i) {
      if (childArgs[i])
        childIdx |= (1u << i);
    }

    // final output
    raw[idx] = childTT.bits().bit(childIdx);
  }

  // 4) Choose constructor based on newSize
  if (newSize <= 6) {
    // pack into uint64_t mask
    uint64_t mask = 0;
    for (uint32_t i = 0; i < Nrows; ++i) {
      if (raw[i]) mask |= (uint64_t{1} << i);
    }
    return SNLTruthTable(newSize, mask);
  }

  // otherwise use vector<bool>
  return SNLTruthTable(newSize, std::move(raw));
}
