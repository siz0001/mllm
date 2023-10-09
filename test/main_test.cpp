#include <iostream>
#include "Net.hpp"
#include "Executor.hpp"
#include "NetParameter.hpp"
#include "express/Express.hpp"

using namespace mllm;
// For Visualization and Debug
void display(NetParameter *net) {
    std::cout << "===NetParameter===" << std::endl;
    for (auto *op : net->net_ops) {
        std::cout << "===NetOP===" << std::endl;
        std::cout << "op->name:" << op->name << std::endl;
        std::cout << "op->type:" << op->type << std::endl;
        std::cout << "op input" << op->in.size() << std::endl;
        for (auto *input : op->in) {
            std::cout << "==Input==\ninput.name:" << input->name << std::endl;
            if (input->in != nullptr) {
                std::cout << "input op:" << input->in->name << std::endl;
            }
            std::cout << "input in subgraph:" << (input->subgraph == net) << std::endl;
            std::cout << std::endl;
        }
        std::cout << "op output" << op->out.size() << std::endl;
        for (auto *output : op->out) {
            std::cout << "output.name:" << output->name << std::endl;
            std::cout << "output op:" << output->out.size() << std::endl;
            if (!output->out.empty()) {
                std::cout << "output op:" << output->out[0]->name << std::endl;
            }
        }
        std::cout << std::endl;
    }
}
void display(Context *c) {
    for (auto sub : c->sub_param_) {
        display(&sub);
    }
}
int main() {
    /*
    //  test model sample
     Context *c = new Context();
     auto* x = _Input(c, {1, 3, 3, 3});
     x = _Softmax(c, {x}, -1);
     auto* z = _SiLU(c, {x});
     _SubgraphBegin(c);
     auto* y = _SiLU(c, {x});
     x = _Matmul(c, {z, y});
     x = _Softmax(c, {x}, -1);
     */

    /*
    // decoder blk sample
    Context *c = new Context();
    auto *in = _Input(c, {1, 1, 1, 1});
    auto *x = _RMSNorm(c, {in});
    auto *q = _Linear(c, {x}, 1, 1, false);
    auto *k = _Linear(c, {x}, 1, 1, false);
    auto *v = _Linear(c, {x}, 1, 1, false);
    auto *o = _Matmul(c, {q, k});
    o = _Softmax(c, {o}, 1);
    o = _Matmul(c, {o, v});
    o = _Linear(c, {o}, 1, 1, false);
    o = _Add(c, {o, in});
    _SubgraphBegin(c);
    x = _RMSNorm(c, {o});
    x = _Linear(c, {x}, 1, 1, false);
    x = _Linear(c, {x}, 1, 1, false);
    o = _Add(c, {o, x});
    */


    Context *c = new Context();
//    auto *x = _Input(c, {1, 32, 1, 1});
//    x = _Linear(c, {x}, 32, 16, false);
//    x = _Softmax(c, {x}, 1);
//    x = _SelfAttention(c, {x}, 32, 32);



    auto *x = _Input(c, {1, 1, 1, 10});
//    x = _RMSNorm(c, {x});
    x = _SelfAttention(c, {x}, 10, 10);

    // display(c);

    BackendConfig bn;

    Net net(c->sub_param_, bn);
    net.convert();
    // net.Run();

    Executor ex(&net);
    ex.execute();
    ex.execute();
    return 0;
}
