#pragma once

#include <cstdint>
#include "serialize.h"
#include "serializer.h"

namespace serde {

namespace detail {
template<typename T>
inline void serialize_signed_integer(Serializer& ser, const T& val) {
  if constexpr (sizeof(std::decay_t<T>) == 1) {
    ser.serialize_i8(val);
  }
  else if constexpr (sizeof(std::decay_t<T>) == 2) {
    ser.serialize_i16(val);
  }
  else if constexpr (sizeof(std::decay_t<T>) == 4) {
    ser.serialize_i32(val);
  }
  else if constexpr (sizeof(std::decay_t<T>) == 8) {
    ser.serialize_i64(val);
  } else {
    static_assert(sizeof(std::decay_t<T>) <= 8, "unsupported signed integer size");
  }
}

template<typename T>
inline void serialize_unsigned_integer(Serializer& ser, const T& val) {
  if constexpr (sizeof(std::decay_t<T>) == 1) {
    ser.serialize_u8(val);
  }
  else if constexpr (sizeof(std::decay_t<T>) == 2) {
    ser.serialize_u16(val);
  }
  else if constexpr (sizeof(std::decay_t<T>) == 4) {
    ser.serialize_u32(val);
  }
  else if constexpr (sizeof(std::decay_t<T>) == 8) {
    ser.serialize_u64(val);
  } else {
    static_assert(sizeof(std::decay_t<T>) <= 8, "unsupported unsigned integer size");
  }
}
} // namespace detail

template<>
inline void serialize(Serializer& ser, const bool& v)
{
  ser.serialize_bool(v);
}

template<>
inline void serialize(Serializer& ser, const int& v)
{
  detail::serialize_signed_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const short int& v)
{
  detail::serialize_signed_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const long int& v)
{
  detail::serialize_signed_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const long long int& v)
{
  detail::serialize_signed_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const unsigned int& v)
{
  detail::serialize_unsigned_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const short unsigned int& v)
{
  detail::serialize_unsigned_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const long unsigned int& v)
{
  detail::serialize_unsigned_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const long long unsigned int& v)
{
  detail::serialize_unsigned_integer(ser, v);
}

template<>
inline void serialize(Serializer& ser, const float& v)
{
  ser.serialize_float(v);
}

template<>
inline void serialize(Serializer& ser, const double& v)
{
  ser.serialize_double(v);
}

template<>
inline void serialize(Serializer& ser, const char& v)
{
  ser.serialize_char(v);
}

template<>
inline void serialize(Serializer& ser, const signed char& v)
{
  ser.serialize_char(v);
}

template<>
inline void serialize(Serializer& ser, const unsigned char& v)
{
  ser.serialize_uchar(v);
}

template<>
inline void serialize(Serializer& ser, char* const& val)
{
  ser.serialize_cstr(val);
}

template<>
inline void serialize(Serializer& ser, const char* const& val)
{
  ser.serialize_cstr(val);
}

template<size_t N>
inline void serialize(Serializer& ser, const char (&val)[N])
{
  ser.serialize_cstr(val);
}

} // namespace serde

