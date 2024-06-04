#include <istream>
#include <type_traits>

class BitReader {
public:
    explicit BitReader(std::istream& istream);

    bool GetNextBit(bool skip_ff = false);

    uint8_t GetNextByte(bool skip_ff = false);

    uint16_t PeekNextBytes();

private:
    std::istream& istream_;
    uint8_t current_byte_{};
    size_t current_bit_{};
};
