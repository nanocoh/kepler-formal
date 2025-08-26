#ifndef SNLTRUTHTABLETREE_H
#define SNLTRUTHTABLETREE_H

#include "SNLTruthTable.h"
#include <vector>
#include <memory>
#include <cstddef>

namespace KEPLER_FORMAL {

/// Abstract base class for a node in the truth-table tree.
struct ITTNode {
    virtual ~ITTNode() = default;
    virtual bool eval(const std::vector<bool>& extInputs) const = 0;
};

/// Leaf node representing one of the external inputs.
struct InputNode : public ITTNode {
    size_t inputIndex;  // which bit of the external vector

    explicit InputNode(size_t idx);
    bool eval(const std::vector<bool>& extInputs) const override;
};

/// Internal node wrapping an SNLTruthTable.
/// Its children feed its inputs in table-input order.
struct TableNode : public ITTNode {
    naja::NL::SNLTruthTable table;
    std::vector<std::unique_ptr<ITTNode>> children;

    explicit TableNode(naja::NL::SNLTruthTable t);
    void addChild(std::unique_ptr<ITTNode> child);
    bool eval(const std::vector<bool>& extInputs) const override;
};

/// Composite wrapper presenting a single n-input interface,
/// but built as a tree of smaller truth-tables.
class SNLTruthTableTree {
public:
    // Default constructor: empty tree, zero inputs
    SNLTruthTableTree();

    SNLTruthTableTree(naja::NL::SNLTruthTable table);
    /// @param root               existing root (InputNode or TableNode)
    /// @param numExternalInputs  how many external bits eval() expects
    SNLTruthTableTree(std::unique_ptr<ITTNode> root,
                      size_t numExternalInputs);

    /// current number of external inputs
    size_t size() const;

    /// evaluate on one external-input vector
    bool eval(const std::vector<bool>& extInputs) const;

    /// graft a new truth-table onto the free leaf-slot numbered
    /// borderIndex.  The old InputNode becomes child 0; any extra
    /// inputs spawn new InputNodes with fresh external indices.
    /// external-input count grows by (table.size()−1).
    void concat(size_t borderIndex, naja::NL::SNLTruthTable table);

    /// expose the root node for external walkers (e.g. Tree2BoolExpr)
    const ITTNode* getRoot() const { return root_.get(); }

    void concatFull(std::vector<naja::NL::SNLTruthTable> tables);

    bool isInitialized() const;

    void print() const; 

private:
    void concatBody(size_t borderIndex, naja::NL::SNLTruthTable table);
    /// descriptor of each free leaf-slot in the tree
    struct BorderLeaf {
        TableNode* parent;  // nullptr if the leaf is the root
        size_t childPos;    // which slot in parent->children
        size_t extIndex;    // the InputNode.inputIndex being replaced
    };

    /// walk the tree and collect every InputNode leaf as a border slot
    void updateBorderLeaves();

    std::unique_ptr<ITTNode>   root_;
    size_t                     numExternalInputs_ = -1;
    std::vector<BorderLeaf>    borderLeaves_;
};

} // namespace naja::NL

#endif // SNLTRUTHTABLETREE_H
