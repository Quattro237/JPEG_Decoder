#include <huffman.h>

struct Node {
    uint8_t value_{};
    bool is_terminal_ = false;
    std::shared_ptr<Node> left_{};
    std::shared_ptr<Node> right_{};
};

class HuffmanTree::Impl {
public:
    Impl(bool arg_built, std::shared_ptr<Node> arg_root, std::shared_ptr<Node> arg_cur_node)
        : built(arg_built), root(arg_root), cur_node(arg_cur_node) {
    }

    bool built = false;
    std::shared_ptr<Node> root{};
    std::shared_ptr<Node> cur_node{};
};

HuffmanTree::HuffmanTree() {
}

bool AddValue(uint8_t value, std::shared_ptr<Node> node, size_t required_level, size_t node_level) {
    if (node->is_terminal_) {
        return false;
    }

    if (required_level == node_level) {
        node->is_terminal_ = true;
        node->value_ = value;
        return true;
    }

    if (node->left_) {
        if (AddValue(value, node->left_, required_level, node_level + 1)) {
            return true;
        } else {
            if (!node->right_) {
                node->right_ = std::make_shared<Node>();
            }
            return AddValue(value, node->right_, required_level, node_level + 1);
        }
    } else {
        node->left_ = std::make_shared<Node>();
        return AddValue(value, node->left_, required_level, node_level + 1);
    }
}

void HuffmanTree::Build(const std::vector<uint8_t> &code_lengths,
                        const std::vector<uint8_t> &values) {
    if (code_lengths.size() > 16) {
        throw std::invalid_argument("Code lengths is too big");
    }

    if (!impl_) {
        impl_ = std::make_unique<Impl>(false, nullptr, nullptr);
    }

    impl_->root = std::make_shared<Node>();
    size_t cur_val = 0;

    for (size_t level = 1; level < code_lengths.size() + 1; ++level) {
        uint8_t cur_level_length = code_lengths[level - 1];
        while (cur_level_length != 0) {
            if (cur_val >= values.size()) {
                throw std::invalid_argument("Incorrect values");
            }
            if (!AddValue(values[cur_val], impl_->root, level, 0)) {
                throw std::invalid_argument("Incorrect code lengths");
            }
            ++cur_val;
            --cur_level_length;
        }
    }

    if (cur_val != values.size()) {
        throw std::invalid_argument("Incorrect values");
    }

    impl_->cur_node = impl_->root;
    impl_->built = true;
}

bool HuffmanTree::Move(bool bit, int &value) {
    if (impl_ == nullptr || impl_->root == nullptr) {
        throw std::invalid_argument("Tree was not built");
    }

    if (bit) {
        if (!impl_->cur_node->right_) {
            throw std::invalid_argument("Invalid move");
        }
        impl_->cur_node = impl_->cur_node->right_;
    } else {
        if (!impl_->cur_node->left_) {
            throw std::invalid_argument("Invalid move");
        }
        impl_->cur_node = impl_->cur_node->left_;
    }

    if (impl_->cur_node->is_terminal_) {
        value = impl_->cur_node->value_;
        impl_->cur_node = impl_->root;
        return true;
    } else {
        return false;
    }
}

HuffmanTree::HuffmanTree(HuffmanTree &&) = default;

HuffmanTree &HuffmanTree::operator=(HuffmanTree &&) = default;

HuffmanTree::~HuffmanTree() = default;
