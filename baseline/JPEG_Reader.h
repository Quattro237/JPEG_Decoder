#include "BitReader.h"
#include "include/huffman.h"
#include "include/fft.h"
#include "include/decoder.h"
#include <fft.h>
#include <cmath>

using DQTTable = std::vector<std::vector<int>>;

enum Markers {
    SECTION_BEGIN_MARKER = 0xFF,
    SOI = 0xD8,
    EOI = 0xD9,
    COM = 0xFE,
    APP,
    START_APP = 0xE0,
    END_APP = 0xEF,
    DQT = 0xDB,
    SOF0 = 0xC0,
    DHT = 0xC4,
    SOS = 0xDA
};

struct ChannelInfo {
    uint8_t horizontal{};
    uint8_t vertical{};
    size_t dqt_table{};
    uint8_t dc_table_idx{};
    uint8_t ac_table_idx{};
};

struct ChannelHandler {
    ChannelHandler(std::vector<double> in, std::vector<double> out)
        : input(std::move(in)), output(std::move(out)), calc(8, &input, &output) {
    }

    std::vector<double> input{};
    std::vector<double> output{};
    DctCalculator calc;
};

class MCU {
public:
    MCU();

    using Block = std::vector<std::vector<double>>;

    std::vector<std::vector<std::vector<Block>>> mcu_{};
};

class JpegReader {
public:
    explicit JpegReader(std::istream& istream);

    Markers GetMarker();

    size_t GetLength();

    int GetNumber(size_t length, bool skip_ff = false);

    void ReadComment(Image& image);

    void ReadApp();

    void ReadDQT();

    void ReadHT();

    void ReadSOF0(Image& image);

    void ReadSOS(Image& image);

    std::vector<int> ReadMatrix(size_t dc_ht_idx, size_t ac_ht_idx);

    void HandleChannels();

    RGB GetRGB(double y, double cb, double cr);

    MCU ReadMCU(size_t channels_cnt);

    void HandleMCU(MCU& mcu, size_t channels_cnt);

    std::vector<std::vector<RGB>> ToRGB(MCU& mcu, size_t channels_cnt);

    double GetComponent(MCU& mcu, size_t channel, size_t i, size_t j);

private:
    BitReader bit_reader_;
    std::vector<DQTTable> dqt_tables_{};
    std::vector<ChannelInfo> channels_info_{};
    std::vector<HuffmanTree> dc_h_ts_{};
    std::vector<HuffmanTree> ac_h_ts_{};
    uint8_t max_h_{};
    uint8_t max_v_{};
    ChannelHandler ch_handler_;
    size_t current_mcu_{};
    std::vector<int> dc_coeffs_{};
};