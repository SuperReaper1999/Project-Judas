#pragma once
#include "../third_party/nlohmann/json.hpp"
// SAX preflight rejects nesting/container bombs before constructing the DOM.
struct BoundedModelSax : nlohmann::json_sax<nlohmann::json> {
  size_t events = 0, depth = 0;
  bool Next() { return ++events <= 24000000; }
  bool null() override { return Next(); }
  bool boolean(bool) override { return Next(); }
  bool number_integer(number_integer_t) override { return Next(); }
  bool number_unsigned(number_unsigned_t) override { return Next(); }
  bool number_float(number_float_t, const string_t &) override {
    return Next();
  }
  bool string(string_t &value) override {
    return Next() && value.size() <= 1048576;
  }
  bool binary(binary_t &value) override {
    return Next() && value.size() <= 128 * 1024 * 1024;
  }
  bool key(string_t &value) override { return string(value); }
  bool start_object(std::size_t n) override {
    return Next() && ++depth <= 64 && (n == size_t(-1) || n <= 6000000);
  }
  bool end_object() override {
    --depth;
    return Next();
  }
  bool start_array(std::size_t n) override { return start_object(n); }
  bool end_array() override { return end_object(); }
  bool parse_error(std::size_t, const std::string &,
                   const nlohmann::detail::exception &) override {
    return false;
  }
};
