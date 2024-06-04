#include <fft.h>

#include <fftw3.h>
#include <cmath>

class DctCalculator::Impl {
public:
    Impl(size_t in_width, std::vector<double> *in_input, std::vector<double> *in_output)
        : width(in_width), input(in_input), output(in_output) {
        if (input->size() != width * width || output->size() != width * width) {
            throw std::invalid_argument("Invalid data");
        }
        plan = fftw_plan_r2r_2d(width, width, &(input->at(0)), &(output->at(0)), FFTW_REDFT01,
                                FFTW_REDFT01, FFTW_ESTIMATE);
    }

    size_t width{};
    std::vector<double> *input{};
    std::vector<double> *output{};
    fftw_plan plan{};

    ~Impl() {
        fftw_destroy_plan(plan);
        fftw_cleanup();
    }
};

DctCalculator::DctCalculator(size_t width, std::vector<double> *input, std::vector<double> *output)
    : impl_(std::make_unique<Impl>(width, input, output)) {
}

void DctCalculator::Inverse() {
    for (size_t i = 0; i < impl_->width * impl_->width; i += impl_->width) {
        impl_->input->at(i) *= sqrt(2);
    }

    for (size_t i = 0; i < impl_->width; ++i) {
        impl_->input->at(i) *= sqrt(2);
    }

    fftw_execute(impl_->plan);

    for (size_t i = 0; i < impl_->width * impl_->width; ++i) {
        impl_->output->at(i) /= 16;
    }
}

DctCalculator::~DctCalculator() = default;
