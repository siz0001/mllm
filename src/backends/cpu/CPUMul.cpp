
#include "CPUMul.hpp"

namespace mllm {

CPUMul::CPUMul(Backend *bn,  string opName, bool multiThread) :
    Op(bn, opName) {
}

ErrorCode CPUMul::reshape(vector<shared_ptr<Tensor>> inputs, vector<shared_ptr<Tensor>> outputs) {
    std::cout<<name() << "  CPUMul  reshape" << std::endl;
    CHECK_EQ(inputs.size(), 2);
    CHECK_EQ(outputs.size(), 1);
    CHECK_EQ(inputs[0]->shape(0), inputs[1]->shape(0));
    CHECK_EQ(inputs[0]->shape(1), inputs[1]->shape(1));
    CHECK_EQ(inputs[0]->shape(2), inputs[1]->shape(2));
    CHECK_EQ(inputs[0]->shape(3), inputs[1]->shape(3));
    outputs[0]->reshape(inputs[0]->shape(0), inputs[0]->shape(1), inputs[0]->shape(2), inputs[0]->shape(3));
    outputs[0]->setDtype(activationDtype());
    return NO_ERROR;
}

ErrorCode CPUMul::execute(vector<shared_ptr<Tensor>> inputs, vector<shared_ptr<Tensor>> outputs) {
    std::cout<<name() << "  CPUMul()" << std::endl;
    int N = inputs[0]->shape(0);
    int C = inputs[0]->shape(1);
    int H = inputs[0]->shape(2);
    int W = inputs[0]->shape(3);
    for (int n = 0; n < N; ++n) {
        for (int c = 0; c < C; ++c) {
            for (int h = 0; h < H; ++h) {
                for (int w = 0; w < W; ++w) {
                    outputs[0]->setDataAt<float>(n, c, h, w, inputs[0]->dataAt<float>(n, c, h, w) * inputs[1]->dataAt<float>(n, c, h, w));
                }
            }
        }
    }
    return NO_ERROR;
}

ErrorCode CPUMul::load(ParamLoader &loader) {
    std::cout<<name() << "  CPUMul load" << std::endl;
    return NO_ERROR;
}
} // namespace mllm
