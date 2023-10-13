#ifndef MLLM_NNAPIBACKEND_H
#define MLLM_NNAPIBACKEND_H

#include "Backend.hpp"
#include "Op.hpp"
#include "Types.hpp"
#include "NNAPIDefine.hpp"
#include "NNAPISymbol.hpp"
#include <cstdint>

namespace mllm {
class NNAPIBackend : public Backend {
public:
    NNAPIBackend(shared_ptr<MemoryManager> mm);
    ~NNAPIBackend();

    class Creator {
    public:
        // virtual Op* Create(const vector<shared_ptr<Tensor>>& inputs, const vector<shared_ptr<Tensor>>& outputs,
        //                             OpParam op_param, Backend* backend) const = 0;
        virtual Op *create(OpParam op_param, Backend *bn) const = 0;
    };
    void initCreatorMap() {
        map_creator_ = new std::map<OpType, NNAPIBackend::Creator *>;
    }
    bool addCreator(OpType t, Creator *c) {
        auto *map = map_creator_;
        if (map->find(t) != map->end()) {
            printf("Error: %d type has be added\n", t);
            return false;
        }
        map->insert(std::make_pair(t, c));
        return true;
    }

    // virtual Op* OpCreate(const vector<shared_ptr<Tensor>>& inputs, const vector<shared_ptr<Tensor>>& outputs,
    //                             OpParam op_param) override;

    virtual Op *opCreate(const OpParam &op_param) override;

    virtual void registerOps() override;

    // NNAPI
    int bytes() const {
#ifdef USE_ARMV82
        return precision_ >= BackendConfig::PrecisionMode::Precision_Low ? 2 : 4;
#else
        return 4;
#endif
    }
    uint32_t getTensorIdx(const Tensor *t, bool dequant = false);
    uint32_t buildOperand(const void *data, size_t size, OperandCode code, std::vector<uint32_t> dims = {}, const float *scales = nullptr, int zero = 0);
    ErrorCode buildOperation(int op, const std::vector<uint32_t> &inputs, const std::vector<uint32_t> &outputs);
    ErrorCode buildModel();
    void invokeModel() const;

private:
    // TODO: precision config
    BackendConfig::PrecisionMode precision_ = BackendConfig::PrecisionMode::Precision_Normal;
    std::map<OpType, NNAPIBackend::Creator *> *map_creator_;
    std::vector<shared_ptr<Tensor>> inputTensors_, outputTensors_;
    std::vector<std::unique_ptr<Tensor>> inputContentTensors_, outputContentTensors_;
    // tensor idx map
    std::map<const Tensor *, uint32_t> tensorIdxMap_, dequantIdxMap_;
    std::map<uint32_t, const Tensor *> dequantMap_;
    uint32_t tensorIdx_ = 0;
    // fp16 buffer
    std::vector<std::unique_ptr<int16_t[]>> halfBuffer_;
    // NNAPI resource
    struct NNAPIDevice {
        ANeuralNetworksDevice *device;
        const char *name;
        int32_t type;
    };
    std::vector<NNAPIDevice> mNNAPIDevices_;
    ANeuralNetworksModel *mNNAPIModel_ = nullptr;
    ANeuralNetworksCompilation *mNNAPICompilation_ = nullptr;
    ANeuralNetworksBurst *mNNAPIBurst_ = NULL;
};

} // namespace mllm

#endif // MLLM_NNAPIBACKEND_H