#pragma once

#include <array>
#include "../deserialize.h"
#include "../deserializer.h"

namespace serde {

template<>
struct DeserializeTN<std::array> {
  template<typename T, auto N>
  static void deserialize(Deserializer& de, std::array<T, N>& arr) {
    de.deserialize_seq_begin();
    for (auto& e : arr)
      de.deserialize(e);
    de.deserialize_seq_end();
  }
};

} // namespace serde

