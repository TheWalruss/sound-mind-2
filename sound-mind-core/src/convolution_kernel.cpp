#include "sound_mind/core/convolution_kernel.h"

namespace sound_mind::core {

void to_json(nlohmann::json& json, const NamedConvolutionKernel& namedKernel) {
    json = nlohmann::json{{"id", namedKernel.id},
                           {"name", namedKernel.name},
                           {"size", namedKernel.size},
                           {"coefficients", namedKernel.coefficients},
                           {"normalize", namedKernel.normalize}};
}

void from_json(const nlohmann::json& json, NamedConvolutionKernel& namedKernel) {
    json.at("id").get_to(namedKernel.id);
    json.at("name").get_to(namedKernel.name);
    json.at("size").get_to(namedKernel.size);
    json.at("coefficients").get_to(namedKernel.coefficients);
    json.at("normalize").get_to(namedKernel.normalize);
}

}  // namespace sound_mind::core
