#include "BitReader.h"

BitReader::BitReader(std::istream& istream) : istream_(istream), current_bit_(8) {
    current_byte_ = istream_.get();
    if (istream.eof()) {
        throw std::invalid_argument("Invalid istream on input");
    }
}

bool BitReader::GetNextBit(bool skip_ff) {
    if (current_bit_ == 0) {
        current_byte_ = istream_.get();
        if (istream_.eof()) {
            throw std::runtime_error("Cannot read next byte");
        }
        if (skip_ff && current_byte_ == 0xFF) {
            istream_.get();
        }
        current_bit_ = 8;
    }

    bool ret_val = current_byte_ & (1 << (current_bit_ - 1));
    --current_bit_;
    return ret_val;
}

uint8_t BitReader::GetNextByte(bool skip_ff) {
    if (current_bit_ == 0) {
        current_byte_ = istream_.get();
        if (istream_.eof()) {
            throw std::runtime_error("Cannot read next byte");
        }
        if (skip_ff && current_byte_ == 0xFF) {
            istream_.get();
        }
        return current_byte_;
    }

    if (current_bit_ == 8) {
        current_bit_ = 0;
        return current_byte_;
    }

    throw std::invalid_argument("Current byte was not read fully");
}

uint16_t BitReader::PeekNextBytes() {
    uint16_t res = 0;
    uint8_t first_byte = 0;
    uint8_t second_byte = 0;

    if (current_bit_ < 8) {
        first_byte = istream_.get();
        if (istream_.eof()) {
            throw std::runtime_error("Cannot read next byte");
        }
    } else {
        first_byte = current_byte_;
    }

    res += first_byte;
    res <<= 8;

    second_byte = istream_.get();
    if (istream_.eof()) {
        throw std::runtime_error("Cannot read next byte");
    }

    res += second_byte;

    istream_.unget();
    if (current_bit_ < 8) {
        istream_.unget();
    }

    return res;
}