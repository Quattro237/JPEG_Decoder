#include "JPEG_Reader.h"
#include "cassert"

#include <glog/logging.h>

namespace {
template <class T>
std::vector<std::vector<T>> ZigZag(const std::vector<T>& data) {
    if (data.size() != 64) {
        throw std::invalid_argument("Invalid size of data for ZigZag");
    }
    size_t i = 0;
    size_t j = 0;
    size_t cur_val = 0;
    std::vector<std::vector<T>> matrix(8, std::vector<T>(8));

    while (j != 6) {
        matrix[i][j] = data[cur_val];
        ++cur_val;
        ++j;

        matrix[i][j] = data[cur_val];
        ++cur_val;

        while (j != 0) {
            ++i;
            --j;
            matrix[i][j] = data[cur_val];
            ++cur_val;
        }

        ++i;
        matrix[i][j] = data[cur_val];
        ++cur_val;

        while (i != 0) {
            --i;
            ++j;
            matrix[i][j] = data[cur_val];
            ++cur_val;
        }
        --cur_val;
    }

    matrix[i][j] = data[cur_val];
    ++cur_val;
    ++j;

    matrix[i][j] = data[cur_val];
    ++cur_val;

    while (j != 0) {
        ++i;
        --j;
        matrix[i][j] = data[cur_val];
        ++cur_val;
    }
    --cur_val;

    while (j != 6) {
        matrix[i][j] = data[cur_val];
        ++cur_val;
        ++j;

        matrix[i][j] = data[cur_val];
        ++cur_val;

        while (j != 7) {
            --i;
            ++j;
            matrix[i][j] = data[cur_val];
            ++cur_val;
        }

        ++i;
        matrix[i][j] = data[cur_val];
        ++cur_val;

        while (i != 7) {
            ++i;
            --j;
            matrix[i][j] = data[cur_val];
            ++cur_val;
        }
        --cur_val;
    }

    ++j;
    ++cur_val;
    matrix[i][j] = data[cur_val];

    return matrix;
}
}  // namespace

JpegReader::JpegReader(std::istream& istream)
    : bit_reader_(istream),
      dc_h_ts_(4),
      ac_h_ts_(4),
      ch_handler_(std::vector<double>(64), std::vector<double>(64)),
      dc_coeffs_(4, 0) {
    DLOG(INFO) << "Constructor";
}

Markers JpegReader::GetMarker() {
    uint8_t section_begin_marker = bit_reader_.GetNextByte();
    if (section_begin_marker != SECTION_BEGIN_MARKER) {
        throw std::runtime_error("Invalid JPEG (section does not start with 0xFF)");
    }

    uint8_t section_marker = bit_reader_.GetNextByte();

    if (START_APP <= section_marker && section_marker <= END_APP) {
        return APP;
    }

    if (section_marker != SOI && section_marker != EOI && section_marker != COM &&
        section_marker != DQT && section_marker != SOF0 && section_marker != DHT &&
        section_marker != SOS) {
        throw std::runtime_error("Invalid JPEG (invalid section marker)");
    }

    return static_cast<Markers>(section_marker);
}

int JpegReader::GetNumber(size_t length, bool skip_ff) {
    if (length == 0) {
        return 0;
    }

    int value = 0;
    for (size_t _ = 0; _ < length; ++_) {
        value <<= 1;
        value |= bit_reader_.GetNextBit(skip_ff);
    }

    if (value & (1 << (length - 1))) {
        return value;
    } else {
        return static_cast<int>(value) - (1 << length) + 1;
    }
}

size_t JpegReader::GetLength() {
    uint16_t length = bit_reader_.GetNextByte();
    length <<= 8;
    length |= bit_reader_.GetNextByte();
    return length - 2;
}

void JpegReader::ReadComment(Image& image) {
    DLOG(INFO) << "Comment";
    size_t comment_length = GetLength();
    std::string comment;
    for (size_t i = 0; i < comment_length; ++i) {
        comment += bit_reader_.GetNextByte();
    }

    image.SetComment(comment);
}

void JpegReader::ReadApp() {
    DLOG(INFO) << "App";
    size_t comment_length = GetLength();
    for (size_t i = 0; i < comment_length; ++i) {
        bit_reader_.GetNextByte();
    }
}

void JpegReader::ReadDQT() {
    DLOG(INFO) << "DQT";
    size_t section_length = GetLength();
    size_t read_bytes = 0;
    while (read_bytes < section_length) {
        uint8_t table_idx = 0;
        uint8_t value_size = 0;
        uint8_t half_byte = bit_reader_.GetNextByte();
        ++read_bytes;
        if (read_bytes > section_length) {
            throw std::invalid_argument("Invalid section length");
        }
        table_idx = half_byte & 0x0F;
        value_size = half_byte & 0xF0;

        if (dqt_tables_.size() < table_idx + 1) {
            dqt_tables_.resize(table_idx + 1);
        }

        std::vector<int> data;
        data.reserve(64);

        for (size_t i = 0; i < 64; ++i) {
            uint16_t value = 0;
            value = bit_reader_.GetNextByte();
            ++read_bytes;
            if (read_bytes > section_length) {
                throw std::invalid_argument("Invalid section length");
            }
            if (value_size) {
                value <<= 8;
                value |= bit_reader_.GetNextByte();
                ++read_bytes;
            }

            data.push_back(value);
        }

        assert(data.size() == 64);
        dqt_tables_[table_idx] = ZigZag(data);
    }
}

void JpegReader::ReadHT() {
    DLOG(INFO) << "HT";
    size_t section_length = GetLength();
    size_t read_bytes = 0;
    while (read_bytes < section_length) {
        uint8_t half_byte = bit_reader_.GetNextByte();
        ++read_bytes;
        size_t table_idx = half_byte & 0x0F;
        size_t table_class = half_byte & 0xF0;

        std::vector<uint8_t> code_lengths;
        code_lengths.reserve(16);

        size_t values_cnt = 0;

        for (size_t i = 0; i < 16; ++i) {
            uint8_t code_length = bit_reader_.GetNextByte();
            ++read_bytes;
            code_lengths.push_back(code_length);
            values_cnt += code_length;
        }

        assert(code_lengths.size() == 16);

        std::vector<uint8_t> values;
        values.reserve(values_cnt);

        for (size_t i = 0; i < values_cnt; ++i) {
            uint8_t value = bit_reader_.GetNextByte();
            ++read_bytes;
            values.push_back(value);
        }
        assert(values.size() == values_cnt);

        if (table_class) {
            if (ac_h_ts_.size() < table_idx + 1) {
                ac_h_ts_.resize(table_idx + 1);
            }
            ac_h_ts_[table_idx].Build(code_lengths, values);
        } else {
            if (dc_h_ts_.size() < table_idx + 1) {
                dc_h_ts_.resize(table_idx + 1);
            }
            dc_h_ts_[table_idx].Build(code_lengths, values);
        }
    }
}

void JpegReader::ReadSOF0(Image& image) {
    DLOG(INFO) << "SOF0";
    if (!channels_info_.empty()) {
        throw std::invalid_argument("Can't read two SOF0 sections");
    }

    size_t section_length = GetLength();
    uint8_t precision = bit_reader_.GetNextByte();

    if (precision != 8) {
        throw std::invalid_argument("Invalid precision");
    }

    size_t height = bit_reader_.GetNextByte();
    height <<= 8;
    height |= bit_reader_.GetNextByte();

    size_t width = bit_reader_.GetNextByte();
    width <<= 8;
    width |= bit_reader_.GetNextByte();

    image.SetSize(width, height);

    uint8_t channels_number = bit_reader_.GetNextByte();

    channels_info_.resize(channels_number + 1);
    for (size_t i = 0; i < channels_number; ++i) {
        uint8_t idx = bit_reader_.GetNextByte();
        uint8_t half_byte = bit_reader_.GetNextByte();
        channels_info_[idx].horizontal = (half_byte & 0xF0) >> 4;
        max_h_ = std::max(max_h_, channels_info_[idx].horizontal);
        channels_info_[idx].vertical = half_byte & 0x0F;
        max_v_ = std::max(max_v_, channels_info_[idx].vertical);
        channels_info_[idx].dqt_table = bit_reader_.GetNextByte();
    }
}

std::vector<int> JpegReader::ReadMatrix(size_t dc_ht_idx, size_t ac_ht_idx) {
    std::vector<int> matrix;
    matrix.reserve(64);

    int dc_coeff_len = 0;
    while (!dc_h_ts_[dc_ht_idx].Move(bit_reader_.GetNextBit(true), dc_coeff_len)) {
    }
    matrix.push_back(GetNumber(dc_coeff_len, true));

    while (matrix.size() < 64) {
        int half_byte = 0;

        while (!ac_h_ts_[ac_ht_idx].Move(bit_reader_.GetNextBit(true), half_byte)) {
        }

        if (half_byte == 0) {
            break;
        }

        size_t zeros_cnt = (half_byte & 0xF0) >> 4;
        size_t ac_coeff_len = half_byte & 0x0F;

        for (size_t _ = 0; _ < zeros_cnt; ++_) {
            matrix.push_back(0);
        }
        matrix.push_back(GetNumber(ac_coeff_len, true));
    }

    while (matrix.size() < 64) {
        matrix.push_back(0);
    }

    return matrix;
}

RGB JpegReader::GetRGB(double y, double cb, double cr) {
    y = std::min(255.0, std::max(0.0, y + 128));
    cb = std::min(255.0, std::max(0.0, cb + 128));
    cr = std::min(255.0, std::max(0.0, cr + 128));

    int r = y + 1.402 * (cr - 128);
    int g = y - 0.34414 * (cb - 128) - 0.71414 * (cr - 128);
    int b = y + 1.772 * (cb - 128);

    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    b = std::min(255, std::max(0, b));

    return {r, g, b};
}

MCU JpegReader::ReadMCU(size_t channels_cnt) {
    MCU mcu;

    for (size_t channel = 1; channel <= channels_cnt; ++channel) {
        for (size_t h = 0; h < channels_info_[channel].vertical; ++h) {
            for (size_t v = 0; v < channels_info_[channel].horizontal; ++v) {
                std::vector<double> data(64);

                int dc_coeff_len = 0;
                while (!dc_h_ts_[channels_info_[channel].dc_table_idx].Move(
                    bit_reader_.GetNextBit(true), dc_coeff_len)) {
                }
                dc_coeffs_[channel] += GetNumber(dc_coeff_len, true);
                data[0] = dc_coeffs_[channel];

                size_t read_values = 1;
                while (read_values < 64) {
                    int half_byte = 0;

                    while (!ac_h_ts_[channels_info_[channel].ac_table_idx].Move(
                        bit_reader_.GetNextBit(true), half_byte)) {
                    }

                    if (half_byte == 0) {
                        break;
                    }

                    size_t zeros_cnt = (half_byte & 0xF0) >> 4;
                    size_t ac_coeff_len = half_byte & 0x0F;

                    for (size_t _ = 0; _ < zeros_cnt; ++_) {
                        data[read_values] = 0;
                        ++read_values;
                    }
                    data[read_values] = GetNumber(ac_coeff_len, true);
                    ++read_values;
                }

                while (read_values < 64) {
                    data[read_values] = 0;
                    ++read_values;
                }

                assert(data.size() == 64);

                std::vector<std::vector<double>> matrix = ZigZag(data);
                mcu.mcu_[channel][h][v] = matrix;
            }
        }
    }

    return mcu;
}

MCU::MCU() {
    mcu_.resize(4);
    for (size_t i = 1; i < 4; ++i) {
        mcu_[i].resize(2);
        for (size_t j = 0; j < 2; ++j) {
            mcu_[i][j].resize(2);
        }
    }
}

void JpegReader::HandleMCU(MCU& mcu, size_t channels_cnt) {
    for (size_t channel = 1; channel <= channels_cnt; ++channel) {
        for (size_t h = 0; h < channels_info_[channel].vertical; ++h) {
            for (size_t v = 0; v < channels_info_[channel].horizontal; ++v) {
                for (size_t i = 0; i < 8; ++i) {
                    for (size_t j = 0; j < 8; ++j) {
                        if (dqt_tables_.size() <= channels_info_[channel].dqt_table) {
                            throw std::runtime_error("DQT table with such idx does not exist");
                        }
                        mcu.mcu_[channel][h][v][i][j] *=
                            dqt_tables_[channels_info_[channel].dqt_table][i][j];
                    }
                }

                for (size_t i = 0; i < 64; ++i) {
                    ch_handler_.input[i] = mcu.mcu_[channel][h][v][i / 8][i % 8];
                }
                ch_handler_.calc.Inverse();
                for (size_t i = 0; i < 64; ++i) {
                    mcu.mcu_[channel][h][v][i / 8][i % 8] = ch_handler_.output[i];
                }
            }
        }
    }
}

double JpegReader::GetComponent(MCU& mcu, size_t channel, size_t h, size_t v) {
    size_t vh = h * channels_info_[channel].vertical / max_v_;
    size_t vv = v * channels_info_[channel].horizontal / max_h_;
    return mcu.mcu_[channel][vh / 8][vv / 8][vh % 8][vv % 8];
}

std::vector<std::vector<RGB>> JpegReader::ToRGB(MCU& mcu, size_t channels_cnt) {
    std::vector<std::vector<RGB>> ans(max_v_ * 8, std::vector<RGB>(max_h_ * 8));

    for (size_t i = 0; i < max_v_ * 8; ++i) {
        for (size_t j = 0; j < max_h_ * 8; ++j) {
            double y = GetComponent(mcu, 1, i, j);
            double cb = channels_cnt > 1 ? GetComponent(mcu, 2, i, j) : 0;
            double cr = channels_cnt > 2 ? GetComponent(mcu, 3, i, j) : 0;
            ans[i][j] = GetRGB(y, cb, cr);
        }
    }

    return ans;
}

void JpegReader::ReadSOS(Image& image) {
    DLOG(INFO) << "SOS";
    size_t section_length = GetLength();
    uint8_t channels_count = bit_reader_.GetNextByte();
    for (size_t i = 0; i < channels_count; ++i) {
        uint8_t channel_idx = bit_reader_.GetNextByte();
        uint8_t half_byte = bit_reader_.GetNextByte();
        if (channels_info_.size() <= channel_idx) {
            throw std::runtime_error("No info about channel with such idx");
        }

        channels_info_[channel_idx].dc_table_idx = (half_byte & 0xF0) >> 4;
        channels_info_[channel_idx].ac_table_idx = half_byte & 0x0F;
    }

    if (bit_reader_.GetNextByte(true) != 0 || bit_reader_.GetNextByte(true) != 0x3F ||
        bit_reader_.GetNextByte(true) != 0) {
        throw std::invalid_argument("Invalid Meta info SOS marker");
    };

    size_t mcu_w = (image.Width() - 1) / (8 * max_h_) + 1;
    size_t mcu_h = (image.Height() - 1) / (8 * max_v_) + 1;

    for (size_t i = 0; i < mcu_h; ++i) {
        for (size_t j = 0; j < mcu_w; ++j) {
            MCU mcu = ReadMCU(channels_count);
            HandleMCU(mcu, channels_count);

            std::vector<std::vector<RGB>> rgb = ToRGB(mcu, channels_count);

            for (size_t y = i * 8 * max_v_; y < std::min(image.Height(), (i + 1) * 8 * max_v_);
                 ++y) {
                for (size_t x = j * 8 * max_h_; x < std::min(image.Width(), (j + 1) * 8 * max_h_);
                     ++x) {
                    size_t by = y - i * 8 * max_v_;
                    size_t bx = x - j * 8 * max_h_;
                    image.SetPixel(y, x, rgb[by][bx]);
                }
            }
        }
    }

    if (bit_reader_.PeekNextBytes() != 0xFFD9) {
        throw std::invalid_argument("File does not end with proper marker");
    }
}