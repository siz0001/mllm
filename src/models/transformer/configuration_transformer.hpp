//
// Created by Rongjie Yi on 24-2-29.
//

#ifndef CONFIGURATION_TRANSFORMER_HPP
#define CONFIGURATION_TRANSFORMER_HPP

using namespace mllm;
using namespace std;

class TransformerNameConfig {
public:
    string _q_proj_name;
    string _k_proj_name;
    string _v_proj_name;
    string _o_proj_name;
    string _up_proj_name;
    string _down_proj_name;
    string _attn_base_name;
    string _ffn_base_name;
    string _attn_norm_name;
    string _ffn_norm_name;
    string _qkv_proj_name;
    string _q_norm_name;
    string _k_norm_name;
};
#endif //CONFIGURATION_TRANSFORMER_HPP
