/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "flashlight/fl/contrib/modules/modules.h"
#include "flashlight/fl/flashlight.h"
#include "flashlight/fl/nn/modules/modules.h"

/**
 * This is example of plugin for language model architecture which is expected
 * the input with size Time x Batch x 1 x 1 and used with the adaptive softmax
 * as criterion (so the last linear layer is absent here)
 * This architecture is using also adaptive embedding and sinusoidal positional
 * embedding.
 */
class lm_adae512_sinpos_l8_h8_fc1024_dp03_ldp0_adsm : public fl::Container {
 public:
  lm_adae512_sinpos_l8_h8_fc1024_dp03_ldp0_adsm(int64_t nLabel) {
    // Time x B x 1 x 1
    std::vector<int> cutoffs = {10000, 50000, (int)nLabel};
    frontend_->add(std::make_shared<fl::AdaptiveEmbedding>(512, cutoffs));
    // nFeature x Time x B x 1
    frontend_->add(
        std::make_shared<fl::SinusoidalPositionEmbedding>(512, 22.63));
    frontend_->add(std::make_shared<fl::Dropout>(0.3));
    // nFeature x Time x Batch x 1
    add(frontend_);
    for (int trIdx = 0; trIdx < 8; trIdx++) {
      auto layer = std::make_shared<fl::Transformer>(
          512, 64, 1024, 8, 0, 0.3, 0., false, true);
      transformers_.push_back(layer);
      add(layer);
    }
  }

  std::vector<fl::Variable> forward(
      const std::vector<fl::Variable>& input) override {
    auto out = input[0];
    auto xSizes = input[1].array();
    // expected input dims T x B x 1 x 1
    int T = out.dims(0), B = out.dims(1);
    auto inputMaxSize = af::tile(af::max(xSizes), 1, B);
    af::array inputNotPaddedSize = af::ceil(xSizes * T / inputMaxSize);
    auto padMask = af::iota(af::dim4(T, 1), af::dim4(1, B)) <
        af::tile(inputNotPaddedSize, T, 1);
    out = frontend_->forward(out);
    for (int trIdx = 0; trIdx < transformers_.size(); trIdx++) {
      out = transformers_[trIdx]->forward({out, fl::noGrad(padMask)}).front();
    }
    return {out};
  }

  std::string prettyString() const override {
    std::ostringstream ss;
    ss << "Model lm_adae512_sinpos_l8_h8_fc1024_dp03_ldp0_adsm: ";
    ss << frontend_->prettyString() << "\n";
    for (int trIdx = 0; trIdx < transformers_.size(); trIdx++) {
      ss << transformers_[trIdx]->prettyString() << "\n";
    }
    return ss.str();
  }

 private:
  lm_adae512_sinpos_l8_h8_fc1024_dp03_ldp0_adsm() = default;

  std::shared_ptr<fl::Sequential> frontend_{std::make_shared<fl::Sequential>()};
  std::vector<std::shared_ptr<fl::Transformer>> transformers_;

  FL_SAVE_LOAD_WITH_BASE(fl::Container, frontend_, transformers_)
};
} // namespace rasrLM

extern "C" fl::Module* createModule(int64_t, int64_t nLabel) {
  auto m =
      std::make_unique<lm_adae512_sinpos_l8_h8_fc1024_dp03_ldp0_adsm>(nLabel);
  return m.release();
}

CEREAL_REGISTER_TYPE(lm_adae512_sinpos_l8_h8_fc1024_dp03_ldp0_adsm)
