#ifndef SNLTRUTHTABLETREE_H
#define SNLTRUTHTABLETREE_H

#include "SNLTruthTable.h"
#include <vector>
#include <memory>
#include <cstddef>

namespace KEPLER_FORMAL {

class SNLTruthTableTree {
public:
  /// A unified node: either an Input leaf or a Table internal node.
  struct Node {
    enum class Type { Input, Table } type;

    // for Input
    size_t inputIndex = 0;

    // for Table
    naja::NL::SNLTruthTable table;
    std::vector<std::unique_ptr<Node>> children;

    // constructors
    explicit Node(size_t idx)
      : type(Type::Input), inputIndex(idx) {}

    explicit Node(naja::NL::SNLTruthTable tt)
      : type(Type::Table), table(std::move(tt)) {}

    // evaluate recursively
    bool eval(const std::vector<bool>& extInputs) const;

    // only valid if type==Table
    void addChild(std::unique_ptr<Node> child) {
      children.push_back(std::move(child));
    }
  };

  // build an empty tree
  SNLTruthTableTree();

  // single-table root
  SNLTruthTableTree(naja::NL::SNLTruthTable table);

  // wrap an existing root
  SNLTruthTableTree(std::unique_ptr<Node> root, size_t numExternalInputs);

  // number of external inputs
  size_t size() const;

  // evaluate the whole tree
  bool eval(const std::vector<bool>& extInputs) const;

  // graft a single table at the given leaf‐slot or external index
  void concat(size_t borderIndex, naja::NL::SNLTruthTable table);

  // graft a batch of tables onto the first N border‐leaves
  void concatFull(std::vector<naja::NL::SNLTruthTable> tables);

  // access to the root node
  const Node* getRoot() const { return root_.get(); }

  bool isInitialized() const;
  void print() const;

private:
  // describe a free leaf in the tree
  struct BorderLeaf {
    Node*  parent;    // nullptr if root
    size_t childPos;  // position within parent->children
    size_t extIndex;  // external‐input index
  };

  void updateBorderLeaves();
  void concatBody(size_t borderIndex, naja::NL::SNLTruthTable table);

  std::unique_ptr<Node>   root_;
  size_t                  numExternalInputs_ = 0;
  std::vector<BorderLeaf> borderLeaves_;
};

} // namespace KEPLER_FORMAL

#endif // SNLTRUTHTABLETREE_H
