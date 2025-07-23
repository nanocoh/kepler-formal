// SimplSatSolver.h
#pragma once

#include "BoolExpr.h"                // your BoolExpr definition
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <memory>

namespace KEPLER_FORMAL {

/**
 * SimplSatSolver
 *
 * A simple, in-memory SAT solver that tries every possible assignment
 * to a set of BoolExpr clauses (in CNF) until it finds one that
 * satisfies them all.
 */
class SimplSatSolver {
public:
    /**
     * @param clauses   Vector of shared_ptr<BoolExpr>, each representing
     *                  a disjunction (clause) in CNF.
     * @param varNames  List of all variable names appearing in those clauses.
     */
    SimplSatSolver(const std::vector<BoolExpr>& clauses)
      : clauses_(clauses)
    {}

    /**
     * Attempt to find a satisfying assignment.
     *
     * @param outAssignment  On return (if true), maps each varName to its bool value.
     * @return  true if satisfiable (and outAssignment is set), false otherwise.
     */
    bool solve(std::unordered_map<std::string,bool>& outAssignment);

private:
    void collectVars(const BoolExpr& e,
                          std::unordered_set<std::string>& s);
    void extractVarNames();
    const std::vector<BoolExpr> clauses_;
    std::vector<std::string>            varNames_;
    size_t                              numVars_;
};

}  // namespace KEPLER_FORMAL
