#include "source/extensions/filters/http/dynamic_modules/filter.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

namespace Envoy {
namespace Extensions {
namespace DynamicModules {
namespace HttpFilters {

class DynamicModuleHttpFilterTest : public testing::Test {
public:
  void SetUp() override {
    filter_ = std::make_unique<DynamicModuleHttpFilter>(nullptr);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
  }

  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  Http::TestRequestHeaderMapImpl request_headers_;
  Http::TestRequestTrailerMapImpl request_trailers_;
  Http::TestResponseHeaderMapImpl response_headers_;
  Http::TestResponseTrailerMapImpl response_trailers_;
  std::unique_ptr<DynamicModuleHttpFilter> filter_;
};

// Parameterized test for get_header_value
using GetHeaderValueCallbackType = size_t (*)(envoy_dynamic_module_type_http_filter_envoy_ptr,
                                              envoy_dynamic_module_type_buffer_module_ptr, size_t,
                                              envoy_dynamic_module_type_buffer_envoy_ptr*, size_t*,
                                              size_t);

class DynamicModuleHttpFilterGetHeaderValueTest
    : public DynamicModuleHttpFilterTest,
      public ::testing::WithParamInterface<GetHeaderValueCallbackType> {};

TEST_P(DynamicModuleHttpFilterGetHeaderValueTest, GetHeaderValue) {
  GetHeaderValueCallbackType callback = GetParam();

  // Test with nullptr accessors.
  envoy_dynamic_module_type_buffer_envoy_ptr result_buffer_ptr;
  size_t result_buffer_length_ptr;
  size_t index = 0;
  const size_t res =
      callback(filter_.get(), nullptr, 0, &result_buffer_ptr, &result_buffer_length_ptr, index);
  EXPECT_EQ(res, 0);
  EXPECT_EQ(result_buffer_ptr, nullptr);
  EXPECT_EQ(result_buffer_length_ptr, 0);

  std::initializer_list<std::pair<std::string, std::string>> headers = {
      {"single", "value"}, {"multi", "value1"}, {"multi", "value2"}};
  Http::TestRequestHeaderMapImpl request_headers{headers};
  Http::TestRequestTrailerMapImpl request_trailers{headers};
  Http::TestResponseHeaderMapImpl response_headers{headers};
  Http::TestResponseTrailerMapImpl response_trailers{headers};
  EXPECT_CALL(decoder_callbacks_, requestHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<RequestHeaderMap>(request_headers)));
  EXPECT_CALL(decoder_callbacks_, requestTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<RequestTrailerMap>(request_trailers)));
  EXPECT_CALL(encoder_callbacks_, responseHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseHeaderMap>(response_headers)));
  EXPECT_CALL(encoder_callbacks_, responseTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseTrailerMap>(response_trailers)));

  // The key is not found.
  std::string key = "nonexistent";
  envoy_dynamic_module_type_buffer_module_ptr key_ptr = key.data();
  size_t key_length = key.size();
  result_buffer_ptr = nullptr;
  result_buffer_length_ptr = 0;
  index = 0;
  const size_t rc = callback(filter_.get(), key_ptr, key_length, &result_buffer_ptr,
                             &result_buffer_length_ptr, index);
  EXPECT_EQ(rc, 0);
  EXPECT_EQ(result_buffer_ptr, nullptr);
  EXPECT_EQ(result_buffer_length_ptr, 0);

  // The key is found for single value.
  key = "single";
  key_ptr = key.data();
  key_length = key.size();

  result_buffer_ptr = nullptr;
  result_buffer_length_ptr = 0;
  index = 0;
  const size_t rc2 = callback(filter_.get(), key_ptr, key_length, &result_buffer_ptr,
                              &result_buffer_length_ptr, index);
  EXPECT_EQ(rc2, 1);
  EXPECT_NE(result_buffer_ptr, nullptr);
  EXPECT_EQ(result_buffer_length_ptr, 5);
  EXPECT_EQ(std::string(result_buffer_ptr, result_buffer_length_ptr), "value");

  // key is found for the single value but index is out of range.
  result_buffer_ptr = nullptr;
  result_buffer_length_ptr = 0;
  index = 1;
  const size_t rc3 = callback(filter_.get(), key_ptr, key_length, &result_buffer_ptr,
                              &result_buffer_length_ptr, index);
  EXPECT_EQ(rc3, 1);
  EXPECT_EQ(result_buffer_ptr, nullptr);
  EXPECT_EQ(result_buffer_length_ptr, 0);

  // The key is found for multiple values.
  key = "multi";
  key_ptr = key.data();
  key_length = key.size();

  result_buffer_ptr = nullptr;
  result_buffer_length_ptr = 0;
  index = 0;
  const size_t rc4 = callback(filter_.get(), key_ptr, key_length, &result_buffer_ptr,
                              &result_buffer_length_ptr, index);
  EXPECT_EQ(rc4, 2);
  EXPECT_NE(result_buffer_ptr, nullptr);
  EXPECT_EQ(result_buffer_length_ptr, 6);
  EXPECT_EQ(std::string(result_buffer_ptr, result_buffer_length_ptr), "value1");

  result_buffer_ptr = nullptr;
  result_buffer_length_ptr = 0;
  index = 1;
  const size_t rc5 = callback(filter_.get(), key_ptr, key_length, &result_buffer_ptr,
                              &result_buffer_length_ptr, index);
  EXPECT_EQ(rc5, 2);
  EXPECT_NE(result_buffer_ptr, nullptr);
  EXPECT_EQ(result_buffer_length_ptr, 6);
  EXPECT_EQ(std::string(result_buffer_ptr, result_buffer_length_ptr), "value2");

  // The key is found for multiple values but index is out of range.
  result_buffer_ptr = nullptr;
  result_buffer_length_ptr = 0;
  index = 2;
  const size_t rc6 = callback(filter_.get(), key_ptr, key_length, &result_buffer_ptr,
                              &result_buffer_length_ptr, index);
  EXPECT_EQ(rc6, 2);
  EXPECT_EQ(result_buffer_ptr, nullptr);
  EXPECT_EQ(result_buffer_length_ptr, 0);
}

INSTANTIATE_TEST_SUITE_P(
    GetHeaderValueTests, DynamicModuleHttpFilterGetHeaderValueTest,
    ::testing::Values(envoy_dynamic_module_callback_http_get_request_header,
                      envoy_dynamic_module_callback_http_get_request_trailer,
                      envoy_dynamic_module_callback_http_get_response_header,
                      envoy_dynamic_module_callback_http_get_response_trailer));

// Parameterized test for set_header_value
using SetHeaderValueCallbackType = bool (*)(envoy_dynamic_module_type_http_filter_envoy_ptr,
                                            envoy_dynamic_module_type_buffer_module_ptr, size_t,
                                            envoy_dynamic_module_type_buffer_module_ptr, size_t);

class DynamicModuleHttpFilterSetHeaderValueTest
    : public DynamicModuleHttpFilterTest,
      public ::testing::WithParamInterface<SetHeaderValueCallbackType> {};

TEST_P(DynamicModuleHttpFilterSetHeaderValueTest, SetHeaderValue) {
  SetHeaderValueCallbackType callback = GetParam();

  // Test with nullptr accessors.
  const std::string key = "key";
  const std::string value = "value";
  envoy_dynamic_module_type_buffer_envoy_ptr key_ptr = const_cast<char*>(key.data());
  size_t key_length = key.size();
  envoy_dynamic_module_type_buffer_envoy_ptr value_ptr = const_cast<char*>(value.data());
  size_t value_length = value.size();
  EXPECT_FALSE(callback(filter_.get(), key_ptr, key_length, value_ptr, value_length));

  std::initializer_list<std::pair<std::string, std::string>> headers = {
      {"single", "value"}, {"multi", "value1"}, {"multi", "value2"}};
  Http::TestRequestHeaderMapImpl request_headers{headers};
  Http::TestRequestTrailerMapImpl request_trailers{headers};
  Http::TestResponseHeaderMapImpl response_headers{headers};
  Http::TestResponseTrailerMapImpl response_trailers{headers};
  EXPECT_CALL(decoder_callbacks_, requestHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<RequestHeaderMap>(request_headers)));
  EXPECT_CALL(decoder_callbacks_, requestTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<RequestTrailerMap>(request_trailers)));
  EXPECT_CALL(encoder_callbacks_, responseHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseHeaderMap>(response_headers)));
  EXPECT_CALL(encoder_callbacks_, responseTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseTrailerMap>(response_trailers)));

  Http::HeaderMap* header_map = nullptr;
  if (callback == &envoy_dynamic_module_callback_http_set_request_header) {
    header_map = &request_headers;
  } else if (callback == &envoy_dynamic_module_callback_http_set_request_trailer) {
    header_map = &request_trailers;
  } else if (callback == &envoy_dynamic_module_callback_http_set_response_header) {
    header_map = &response_headers;
  } else if (callback == &envoy_dynamic_module_callback_http_set_response_trailer) {
    header_map = &response_trailers;
  } else {
    FAIL();
  }

  // Non existing key.
  const std::string new_key = "new_one";
  const std::string new_value = "value";
  envoy_dynamic_module_type_buffer_envoy_ptr new_key_ptr = const_cast<char*>(new_key.data());
  size_t new_key_length = new_key.size();
  envoy_dynamic_module_type_buffer_envoy_ptr new_value_ptr = const_cast<char*>(new_value.data());
  size_t new_value_length = new_value.size();
  EXPECT_TRUE(
      callback(filter_.get(), new_key_ptr, new_key_length, new_value_ptr, new_value_length));

  auto values = header_map->get(Envoy::Http::LowerCaseString(new_key));
  EXPECT_EQ(values.size(), 1);
  EXPECT_EQ(values[0]->value().getStringView(), new_value);

  // Existing non-multi key.
  const std::string key2 = "single";
  const std::string value2 = "new_value";
  envoy_dynamic_module_type_buffer_envoy_ptr key_ptr2 = const_cast<char*>(key2.data());
  size_t key_length2 = key2.size();
  envoy_dynamic_module_type_buffer_envoy_ptr value_ptr2 = const_cast<char*>(value2.data());
  size_t value_length2 = value2.size();
  EXPECT_TRUE(callback(filter_.get(), key_ptr2, key_length2, value_ptr2, value_length2));

  auto values2 = header_map->get(Envoy::Http::LowerCaseString(key2));
  EXPECT_EQ(values2.size(), 1);
  EXPECT_EQ(values2[0]->value().getStringView(), value2);

  // Existing multi key must be replaced by a single value.
  const std::string key3 = "multi";
  const std::string value3 = "new_value";
  envoy_dynamic_module_type_buffer_envoy_ptr key_ptr3 = const_cast<char*>(key3.data());
  size_t key_length3 = key3.size();
  envoy_dynamic_module_type_buffer_envoy_ptr value_ptr3 = const_cast<char*>(value3.data());
  size_t value_length3 = value3.size();
  EXPECT_TRUE(callback(filter_.get(), key_ptr3, key_length3, value_ptr3, value_length3));

  auto values3 = header_map->get(Envoy::Http::LowerCaseString(key3));
  EXPECT_EQ(values3.size(), 1);
  EXPECT_EQ(values3[0]->value().getStringView(), value3);

  // Remove the key by passing null value.
  const std::string remove_key = "single";
  envoy_dynamic_module_type_buffer_envoy_ptr remove_key_ptr = const_cast<char*>(remove_key.data());
  size_t remove_key_length = remove_key.size();
  EXPECT_TRUE(callback(filter_.get(), remove_key_ptr, remove_key_length, nullptr, 0));
  auto removed_values = header_map->get(Envoy::Http::LowerCaseString(remove_key));
  EXPECT_EQ(removed_values.size(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    SetHeaderValueTests, DynamicModuleHttpFilterSetHeaderValueTest,
    ::testing::Values(envoy_dynamic_module_callback_http_set_request_header,
                      envoy_dynamic_module_callback_http_set_request_trailer,
                      envoy_dynamic_module_callback_http_set_response_header,
                      envoy_dynamic_module_callback_http_set_response_trailer));

// Parameterized test for get_headers_count
using GetHeadersCountCallbackType = size_t (*)(envoy_dynamic_module_type_http_filter_envoy_ptr);

class DynamicModuleHttpFilterGetHeadersCountTest
    : public DynamicModuleHttpFilterTest,
      public ::testing::WithParamInterface<GetHeadersCountCallbackType> {};

TEST_P(DynamicModuleHttpFilterGetHeadersCountTest, GetHeadersCount) {
  GetHeadersCountCallbackType callback = GetParam();

  // Test with nullptr accessors.
  EXPECT_EQ(callback(filter_.get()), 0);

  std::initializer_list<std::pair<std::string, std::string>> headers = {
      {"single", "value"}, {"multi", "value1"}, {"multi", "value2"}};
  Http::TestRequestHeaderMapImpl request_headers{headers};
  Http::TestRequestTrailerMapImpl request_trailers{headers};
  Http::TestResponseHeaderMapImpl response_headers{headers};
  Http::TestResponseTrailerMapImpl response_trailers{headers};
  EXPECT_CALL(decoder_callbacks_, requestHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<RequestHeaderMap>(request_headers)));
  EXPECT_CALL(decoder_callbacks_, requestTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<RequestTrailerMap>(request_trailers)));
  EXPECT_CALL(encoder_callbacks_, responseHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseHeaderMap>(response_headers)));
  EXPECT_CALL(encoder_callbacks_, responseTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseTrailerMap>(response_trailers)));
  EXPECT_EQ(callback(filter_.get()), 3);
}

INSTANTIATE_TEST_SUITE_P(
    GetHeadersCountTests, DynamicModuleHttpFilterGetHeadersCountTest,
    ::testing::Values(envoy_dynamic_module_callback_http_get_request_headers_count,
                      envoy_dynamic_module_callback_http_get_request_trailers_count,
                      envoy_dynamic_module_callback_http_get_response_headers_count,
                      envoy_dynamic_module_callback_http_get_response_trailers_count));

// Parameterized test for get_headers
using GetHeadersCallbackType = bool (*)(envoy_dynamic_module_type_http_filter_envoy_ptr,
                                        envoy_dynamic_module_type_http_header*);

class DynamicModuleHttpFilterGetHeadersTest
    : public DynamicModuleHttpFilterTest,
      public ::testing::WithParamInterface<GetHeadersCallbackType> {};

TEST_P(DynamicModuleHttpFilterGetHeadersTest, GetHeaders) {
  GetHeadersCallbackType callback = GetParam();

  // Test with nullptr accessors.
  envoy_dynamic_module_type_http_header result_headers[3];
  EXPECT_FALSE(callback(filter_.get(), result_headers));

  std::initializer_list<std::pair<std::string, std::string>> headers = {
      {"single", "value"}, {"multi", "value1"}, {"multi", "value2"}};
  Http::TestRequestHeaderMapImpl request_headers{headers};
  EXPECT_CALL(decoder_callbacks_, requestHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<RequestHeaderMap>(request_headers)));
  Http::TestRequestTrailerMapImpl request_trailers{headers};
  EXPECT_CALL(decoder_callbacks_, requestTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<RequestTrailerMap>(request_trailers)));
  Http::TestResponseHeaderMapImpl response_headers{headers};
  EXPECT_CALL(encoder_callbacks_, responseHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseHeaderMap>(response_headers)));
  Http::TestResponseTrailerMapImpl response_trailers{headers};
  EXPECT_CALL(encoder_callbacks_, responseTrailers())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseTrailerMap>(response_trailers)));

  EXPECT_TRUE(callback(filter_.get(), result_headers));

  EXPECT_EQ(result_headers[0].key_length, 6);
  EXPECT_EQ(std::string(result_headers[0].key_ptr, result_headers[0].key_length), "single");
  EXPECT_EQ(result_headers[0].value_length, 5);
  EXPECT_EQ(std::string(result_headers[0].value_ptr, result_headers[0].value_length), "value");

  EXPECT_EQ(result_headers[1].key_length, 5);
  EXPECT_EQ(std::string(result_headers[1].key_ptr, result_headers[1].key_length), "multi");
  EXPECT_EQ(result_headers[1].value_length, 6);
  EXPECT_EQ(std::string(result_headers[1].value_ptr, result_headers[1].value_length), "value1");

  EXPECT_EQ(result_headers[2].key_length, 5);
  EXPECT_EQ(std::string(result_headers[2].key_ptr, result_headers[2].key_length), "multi");
  EXPECT_EQ(result_headers[2].value_length, 6);
  EXPECT_EQ(std::string(result_headers[2].value_ptr, result_headers[2].value_length), "value2");
}

INSTANTIATE_TEST_SUITE_P(
    GetHeadersTests, DynamicModuleHttpFilterGetHeadersTest,
    ::testing::Values(envoy_dynamic_module_callback_http_get_request_headers,
                      envoy_dynamic_module_callback_http_get_request_trailers,
                      envoy_dynamic_module_callback_http_get_response_headers,
                      envoy_dynamic_module_callback_http_get_response_trailers));

TEST_F(DynamicModuleHttpFilterTest, SendResponseNullptr) {
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Envoy::Http::Code::OK, testing::Eq(""), _,
                                                 testing::Eq(0), testing::Eq("dynamic_module")));
  envoy_dynamic_module_callback_http_send_response(filter_.get(), 200, nullptr, 3, nullptr, 0);
}

TEST_F(DynamicModuleHttpFilterTest, SendResponseEmptyResponse) {
  Http::TestResponseHeaderMapImpl response_headers;
  EXPECT_CALL(encoder_callbacks_, responseHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<ResponseHeaderMap>(response_headers)));

  // Test with empty response.
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Envoy::Http::Code::OK, testing::Eq(""), _,
                                                 testing::Eq(0), testing::Eq("dynamic_module")));
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _));

  envoy_dynamic_module_callback_http_send_response(filter_.get(), 200, nullptr, 3, nullptr, 0);
}

TEST_F(DynamicModuleHttpFilterTest, SendResponse) {
  std::list<std::pair<std::string, std::string>> headers = {
      {"single", "value"}, {"multi", "value1"}, {"multi", "value2"}};
  size_t header_count = headers.size();
  auto header_array =
      std::make_unique<envoy_dynamic_module_type_module_http_header[]>(header_count);

  size_t index = 0;
  for (const auto& [key, value] : headers) {
    header_array[index].key_length = key.size();
    header_array[index].key_ptr = const_cast<char*>(key.c_str());
    header_array[index].value_length = value.size();
    header_array[index].value_ptr = const_cast<char*>(value.c_str());
    ++index;
  }
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Envoy::Http::Code::OK, testing::Eq(""), _,
                                                 testing::Eq(0), testing::Eq("dynamic_module")));
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _)).WillOnce(Invoke([](auto& headers, auto) {
    EXPECT_EQ(headers.get(Http::LowerCaseString("single"))[0]->value().getStringView(), "value");
    EXPECT_EQ(headers.get(Http::LowerCaseString("multi"))[0]->value().getStringView(), "value1");
    EXPECT_EQ(headers.get(Http::LowerCaseString("multi"))[1]->value().getStringView(), "value2");
  }));

  envoy_dynamic_module_callback_http_send_response(filter_.get(), 200, header_array.get(),
                                                   header_count, nullptr, 0);
}

TEST_F(DynamicModuleHttpFilterTest, SendResponseWithBody) {
  std::list<std::pair<std::string, std::string>> headers = {
      {"single", "value"}, {"multi", "value1"}, {"multi", "value2"}};
  size_t header_count = headers.size();
  auto header_array =
      std::make_unique<envoy_dynamic_module_type_module_http_header[]>(header_count);

  size_t index = 0;
  for (const auto& [key, value] : headers) {
    header_array[index].key_length = key.size();
    header_array[index].key_ptr = const_cast<char*>(key.c_str());
    header_array[index].value_length = value.size();
    header_array[index].value_ptr = const_cast<char*>(value.c_str());
    ++index;
  }

  const std::string body_str = "body";
  envoy_dynamic_module_type_buffer_module_ptr body = const_cast<char*>(body_str.data());
  size_t body_length = body_str.size();
  EXPECT_CALL(decoder_callbacks_, sendLocalReply(Envoy::Http::Code::OK, testing::Eq("body"), _,
                                                 testing::Eq(0), testing::Eq("dynamic_module")));
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _)).WillOnce(Invoke([](auto& headers, auto) {
    EXPECT_EQ(headers.get(Http::LowerCaseString("single"))[0]->value().getStringView(), "value");
    EXPECT_EQ(headers.get(Http::LowerCaseString("multi"))[0]->value().getStringView(), "value1");
    EXPECT_EQ(headers.get(Http::LowerCaseString("multi"))[1]->value().getStringView(), "value2");
  }));
  envoy_dynamic_module_callback_http_send_response(filter_.get(), 200, header_array.get(), 3, body,
                                                   body_length);
}

TEST(ABIImpl, metadata) {
  DynamicModuleHttpFilter filter{nullptr};
  const std::string namespace_str = "foo";
  const std::string key_str = "key";
  envoy_dynamic_module_type_buffer_module_ptr namespace_ptr =
      const_cast<char*>(namespace_str.data());
  size_t namespace_length = namespace_str.size();
  envoy_dynamic_module_type_buffer_module_ptr key_ptr = const_cast<char*>(key_str.data());
  size_t key_length = key_str.size();
  double value = 42;
  const std::string value_str = "value";
  envoy_dynamic_module_type_buffer_module_ptr value_ptr = const_cast<char*>(value_str.data());
  size_t value_length = value_str.size();
  double result_number = 0;
  char* result_str_ptr = nullptr;
  size_t result_str_length = 0;

  // No stream info.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_set_dynamic_metadata_number(
      &filter, namespace_ptr, namespace_length, key_ptr, key_length, value));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_set_dynamic_metadata_string(
      &filter, namespace_ptr, namespace_length, key_ptr, key_length, value_ptr, value_length));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_number(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_number));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_str_ptr, &result_str_length));

  // No namespace.
  Http::MockStreamDecoderFilterCallbacks callbacks;
  StreamInfo::MockStreamInfo stream_info;
  EXPECT_CALL(callbacks, streamInfo()).WillRepeatedly(testing::ReturnRef(stream_info));
  envoy::config::core::v3::Metadata metadata;
  EXPECT_CALL(stream_info, dynamicMetadata()).WillRepeatedly(testing::ReturnRef(metadata));
  EXPECT_CALL(callbacks, clusterInfo()).WillRepeatedly(testing::Return(nullptr));
  EXPECT_CALL(stream_info, route()).WillRepeatedly(testing::Return(nullptr));
  EXPECT_CALL(stream_info, upstreamInfo()).WillRepeatedly(testing::Return(nullptr));
  EXPECT_CALL(testing::Const(stream_info), dynamicMetadata())
      .WillRepeatedly(testing::ReturnRef(metadata));
  filter.setDecoderFilterCallbacks(callbacks);
  // Only tests get methods as setters create the namespace.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_number(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_number));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_str_ptr, &result_str_length));
  // Test no metadata on all sources.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Cluster, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_str_ptr, &result_str_length));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Route, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_str_ptr, &result_str_length));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Host, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_str_ptr, &result_str_length));

  // With namespace but non existing key.
  const char* non_existing_key = "non_existing";
  envoy_dynamic_module_type_buffer_module_ptr non_existing_key_ptr =
      const_cast<char*>(non_existing_key);
  size_t non_existing_key_length = strlen(non_existing_key);
  // This will create the namespace.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_set_dynamic_metadata_number(
      &filter, namespace_ptr, namespace_length, key_ptr, key_length, value));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_number(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      non_existing_key_ptr, non_existing_key_length, &result_number));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      non_existing_key_ptr, non_existing_key_length, &result_str_ptr, &result_str_length));

  // With namespace and key.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_set_dynamic_metadata_number(
      &filter, namespace_ptr, namespace_length, key_ptr, key_length, value));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_metadata_number(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_number));
  EXPECT_EQ(result_number, value);
  // Wrong type.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_str_ptr, &result_str_length));

  EXPECT_TRUE(envoy_dynamic_module_callback_http_set_dynamic_metadata_string(
      &filter, namespace_ptr, namespace_length, key_ptr, key_length, value_ptr, value_length));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, value_length);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), value_str);
  // Wrong type.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_metadata_number(
      &filter, envoy_dynamic_module_type_metadata_source_Dynamic, namespace_ptr, namespace_length,
      key_ptr, key_length, &result_number));

  // lbEndpoints metadata.
  const std::string lbendpoint_key = "lbendpoint_key";
  const std::string lbendpoint_value = "lbendpoint_value";
  auto upstream_info = std::make_shared<StreamInfo::MockUpstreamInfo>();
  auto upstream_host = std::make_shared<Upstream::MockHostDescription>();
  EXPECT_CALL(*upstream_info, upstreamHost).WillRepeatedly(testing::Return(upstream_host));
  auto locality_metadata = std::make_shared<envoy::config::core::v3::Metadata>();
  locality_metadata->mutable_filter_metadata()->insert({namespace_str, ProtobufWkt::Struct()});
  ProtobufWkt::Value lbendpoint_value_proto;
  lbendpoint_value_proto.set_string_value(lbendpoint_value);
  locality_metadata->mutable_filter_metadata()
      ->at(namespace_str)
      .mutable_fields()
      ->insert({lbendpoint_key, lbendpoint_value_proto});
  EXPECT_CALL(*upstream_host, localityMetadata())
      .WillRepeatedly(testing::Return(locality_metadata));
  EXPECT_CALL(stream_info, upstreamInfo()).WillRepeatedly(testing::Return(upstream_info));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_metadata_string(
      &filter, envoy_dynamic_module_type_metadata_source_HostLocality, namespace_ptr,
      namespace_length, const_cast<char*>(lbendpoint_key.data()), lbendpoint_key.size(),
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, lbendpoint_value.size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), lbendpoint_value);
}

TEST(ABIImpl, filter_state) {
  DynamicModuleHttpFilter filter{nullptr};
  const std::string key_str = "key";
  envoy_dynamic_module_type_buffer_module_ptr key_ptr = const_cast<char*>(key_str.data());
  size_t key_length = key_str.size();
  const std::string value_str = "value";
  envoy_dynamic_module_type_buffer_module_ptr value_ptr = const_cast<char*>(value_str.data());
  size_t value_length = value_str.size();
  char* result_str_ptr = nullptr;
  size_t result_str_length = 0;

  // No stream info.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_set_filter_state_bytes(
      &filter, key_ptr, key_length, value_ptr, value_length));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_filter_state_bytes(
      &filter, key_ptr, key_length, &result_str_ptr, &result_str_length));

  // With stream info but non existing key.
  const char* non_existing_key = "non_existing";
  envoy_dynamic_module_type_buffer_module_ptr non_existing_key_ptr =
      const_cast<char*>(non_existing_key);
  size_t non_existing_key_length = strlen(non_existing_key);
  Http::MockStreamDecoderFilterCallbacks callbacks;
  StreamInfo::MockStreamInfo stream_info;
  EXPECT_CALL(callbacks, streamInfo()).WillRepeatedly(testing::ReturnRef(stream_info));
  EXPECT_CALL(stream_info, filterState())
      .WillRepeatedly(testing::ReturnRef(stream_info.filter_state_));
  filter.setDecoderFilterCallbacks(callbacks);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_set_filter_state_bytes(
      &filter, key_ptr, key_length, value_ptr, value_length));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_filter_state_bytes(
      &filter, non_existing_key_ptr, non_existing_key_length, &result_str_ptr, &result_str_length));

  // With key.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_filter_state_bytes(
      &filter, key_ptr, key_length, &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, value_length);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), value_str);
}

std::string
bufferVectorToString(const std::vector<envoy_dynamic_module_type_envoy_buffer>& buffer_vector) {
  std::string result;
  for (const auto& buffer : buffer_vector) {
    result.append(buffer.ptr, buffer.length);
  }
  return result;
}

TEST(ABIImpl, RequestBody) {
  DynamicModuleHttpFilter filter{nullptr};
  Http::MockStreamDecoderFilterCallbacks callbacks;
  StreamInfo::MockStreamInfo stream_info;
  EXPECT_CALL(callbacks, streamInfo()).WillRepeatedly(testing::ReturnRef(stream_info));
  filter.setDecoderFilterCallbacks(callbacks);

  size_t length = 0;

  // Non existing buffer should return false.
  EXPECT_CALL(callbacks, decodingBuffer()).WillRepeatedly(testing::ReturnNull());
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_request_body_vector(&filter, nullptr));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_request_body_vector_size(&filter, &length));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_append_request_body(&filter, nullptr, 0));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_drain_request_body(&filter, 0));

  Buffer::OwnedImpl buffer;
  EXPECT_CALL(callbacks, decodingBuffer()).WillRepeatedly(testing::Return(&buffer));
  EXPECT_CALL(callbacks, modifyDecodingBuffer(_))
      .WillRepeatedly(Invoke(
          [&](std::function<void(Buffer::Instance&)> callback) -> void { callback(buffer); }));

  // Empty buffer should return size 0 and drain should return work without problems.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_request_body_vector_size(&filter, &length));
  EXPECT_EQ(length, 0);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_drain_request_body(&filter, 0));

  // Append data to the buffer.
  const std::string data = "foo";
  envoy_dynamic_module_type_buffer_module_ptr data_ptr = const_cast<char*>(data.data());
  size_t data_length = data.size();
  EXPECT_TRUE(
      envoy_dynamic_module_callback_http_append_request_body(&filter, data_ptr, data_length));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_request_body_vector_size(&filter, &length));
  EXPECT_EQ(buffer.toString(), data);

  // Get the data from the buffer.
  auto result_buffer_vector = std::vector<envoy_dynamic_module_type_envoy_buffer>(length);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_request_body_vector(
      &filter, result_buffer_vector.data()));
  EXPECT_EQ(bufferVectorToString(result_buffer_vector), data);

  // Add more data to the buffer.
  const std::string data2 = "bar";
  const std::string data3 = "baz";
  envoy_dynamic_module_type_buffer_module_ptr data_ptr2 = const_cast<char*>(data2.data());
  size_t data_length2 = data2.size();
  envoy_dynamic_module_type_buffer_module_ptr data_ptr3 = const_cast<char*>(data3.data());
  size_t data_length3 = data3.size();
  EXPECT_TRUE(
      envoy_dynamic_module_callback_http_append_request_body(&filter, data_ptr2, data_length2));
  EXPECT_TRUE(
      envoy_dynamic_module_callback_http_append_request_body(&filter, data_ptr3, data_length3));
  EXPECT_EQ(buffer.toString(), data + data2 + data3);

  // Check the data.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_request_body_vector_size(&filter, &length));
  auto result_buffer_vector2 = std::vector<envoy_dynamic_module_type_envoy_buffer>(length);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_request_body_vector(
      &filter, result_buffer_vector2.data()));
  EXPECT_EQ(bufferVectorToString(result_buffer_vector2), data + data2 + data3);

  // Drain the first 5 bytes.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_drain_request_body(&filter, 5));

  // Check the data.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_request_body_vector_size(&filter, &length));
  auto result_buffer_vector3 = std::vector<envoy_dynamic_module_type_envoy_buffer>(length);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_request_body_vector(
      &filter, result_buffer_vector3.data()));
  EXPECT_EQ(bufferVectorToString(result_buffer_vector3), "rbaz");
}

TEST(ABIImpl, ResponseBody) {
  DynamicModuleHttpFilter filter{nullptr};
  Http::MockStreamEncoderFilterCallbacks callbacks;
  StreamInfo::MockStreamInfo stream_info;
  EXPECT_CALL(callbacks, streamInfo()).WillRepeatedly(testing::ReturnRef(stream_info));
  filter.setEncoderFilterCallbacks(callbacks);

  size_t length = 0;

  // Non existing buffer should return false.
  EXPECT_CALL(callbacks, encodingBuffer()).WillRepeatedly(testing::ReturnNull());
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_response_body_vector(&filter, nullptr));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_get_response_body_vector_size(&filter, &length));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_append_response_body(&filter, nullptr, 0));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_drain_response_body(&filter, 0));

  // Buffer is available via current_response_body_, not the stream encoder.
  const std::string data = "foo";
  Buffer::OwnedImpl current_buffer;
  filter.current_response_body_ = &current_buffer;
  EXPECT_TRUE(envoy_dynamic_module_callback_http_append_response_body(
      &filter, const_cast<char*>(data.data()), 3));
  EXPECT_EQ(current_buffer.toString(), data);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_drain_response_body(&filter, 3));
  EXPECT_EQ(current_buffer.toString(), "");
  filter.current_response_body_ = nullptr;

  Buffer::OwnedImpl buffer;
  EXPECT_CALL(callbacks, encodingBuffer()).WillRepeatedly(testing::Return(&buffer));
  EXPECT_CALL(callbacks, modifyEncodingBuffer(_))
      .WillRepeatedly(Invoke(
          [&](std::function<void(Buffer::Instance&)> callback) -> void { callback(buffer); }));

  // Empty buffer should return size 0 and drain should return work without problems.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_response_body_vector_size(&filter, &length));
  EXPECT_EQ(length, 0);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_drain_response_body(&filter, 0));

  // Append data to the buffer.
  envoy_dynamic_module_type_buffer_module_ptr data_ptr = const_cast<char*>(data.data());
  size_t data_length = data.size();
  EXPECT_TRUE(
      envoy_dynamic_module_callback_http_append_response_body(&filter, data_ptr, data_length));
  EXPECT_EQ(buffer.toString(), data);

  // Get the data from the buffer.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_response_body_vector_size(&filter, &length));
  auto result_buffer_vector = std::vector<envoy_dynamic_module_type_envoy_buffer>(length);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_response_body_vector(
      &filter, result_buffer_vector.data()));
  EXPECT_EQ(bufferVectorToString(result_buffer_vector), data);

  // Add more data to the buffer.
  const std::string data2 = "bar";
  const std::string data3 = "baz";
  envoy_dynamic_module_type_buffer_module_ptr data_ptr2 = const_cast<char*>(data2.data());
  size_t data_length2 = data2.size();
  envoy_dynamic_module_type_buffer_module_ptr data_ptr3 = const_cast<char*>(data3.data());
  size_t data_length3 = data3.size();
  EXPECT_TRUE(
      envoy_dynamic_module_callback_http_append_response_body(&filter, data_ptr2, data_length2));
  EXPECT_TRUE(
      envoy_dynamic_module_callback_http_append_response_body(&filter, data_ptr3, data_length3));
  EXPECT_EQ(buffer.toString(), data + data2 + data3);

  // Check the data.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_response_body_vector_size(&filter, &length));
  auto result_buffer_vector2 = std::vector<envoy_dynamic_module_type_envoy_buffer>(length);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_response_body_vector(
      &filter, result_buffer_vector2.data()));
  EXPECT_EQ(bufferVectorToString(result_buffer_vector2), data + data2 + data3);

  // Drain the first 5 bytes.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_drain_response_body(&filter, 5));

  // Check the data.
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_response_body_vector_size(&filter, &length));
  auto result_buffer_vector3 = std::vector<envoy_dynamic_module_type_envoy_buffer>(length);
  EXPECT_TRUE(envoy_dynamic_module_callback_http_get_response_body_vector(
      &filter, result_buffer_vector3.data()));
  EXPECT_EQ(bufferVectorToString(result_buffer_vector3), "rbaz");
}

TEST(ABIImpl, ClearRouteCache) {
  DynamicModuleHttpFilter filter{nullptr};
  Http::MockStreamDecoderFilterCallbacks callbacks;
  StreamInfo::MockStreamInfo stream_info;
  EXPECT_CALL(callbacks, streamInfo()).WillRepeatedly(testing::ReturnRef(stream_info));
  filter.setDecoderFilterCallbacks(callbacks);
  Http::MockDownstreamStreamFilterCallbacks downstream_callbacks;
  EXPECT_CALL(downstream_callbacks, clearRouteCache());
  EXPECT_CALL(callbacks, downstreamCallbacks())
      .WillOnce(testing::Return(OptRef(downstream_callbacks)));
  envoy_dynamic_module_callback_http_clear_route_cache(&filter);
}

TEST(ABIImpl, GetAttributes) {
  DynamicModuleHttpFilter filter_without_callbacks{nullptr};
  DynamicModuleHttpFilter filter{nullptr};
  Http::MockStreamDecoderFilterCallbacks callbacks;
  StreamInfo::MockStreamInfo stream_info;
  EXPECT_CALL(callbacks, streamInfo()).WillRepeatedly(testing::ReturnRef(stream_info));
  envoy::config::core::v3::Metadata metadata;
  EXPECT_CALL(stream_info, dynamicMetadata()).WillRepeatedly(testing::ReturnRef(metadata));
  filter.setDecoderFilterCallbacks(callbacks);
  EXPECT_CALL(stream_info, protocol()).WillRepeatedly(testing::Return(Http::Protocol::Http11));
  EXPECT_CALL(stream_info, upstreamInfo()).Times(testing::AtLeast(1));
  EXPECT_CALL(stream_info, responseCode()).WillRepeatedly(testing::Return(200));
  StreamInfo::StreamIdProviderImpl id_provider("ffffffff-0012-0110-00ff-0c00400600ff");
  EXPECT_CALL(stream_info, getStreamIdProvider())
      .WillRepeatedly(testing::Return(makeOptRef<const StreamInfo::StreamIdProvider>(id_provider)));
  const std::string route_name{"test_route"};
  EXPECT_CALL(stream_info, getRouteName()).WillRepeatedly(testing::ReturnRef(route_name));

  NiceMock<StreamInfo::MockStreamInfo> info;
  EXPECT_CALL(stream_info, downstreamAddressProvider())
      .WillRepeatedly(testing::ReturnPointee(info.downstream_connection_info_provider_));
  info.downstream_connection_info_provider_->setRemoteAddress(
      Envoy::Network::Utility::parseInternetAddressNoThrow("1.1.1.1", 1234, false));
  info.downstream_connection_info_provider_->setLocalAddress(
      Envoy::Network::Utility::parseInternetAddressNoThrow("127.0.0.2", 4321, false));

  char* result_str_ptr = nullptr;
  size_t result_str_length = 0;
  uint64_t result_number = 0;

  // envoy_dynamic_module_type_attribute_id_RequestPath with null headers map, should return false.
  EXPECT_CALL(callbacks, requestHeaders()).WillOnce(testing::Return(absl::nullopt));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestPath, &result_str_ptr,
      &result_str_length));

  std::initializer_list<std::pair<std::string, std::string>> headers = {
      {":path", "/api/v1/action?param=value"},
      {":scheme", "https"},
      {":method", "GET"},
      {":authority", "example.org"},
      {"referer", "envoyproxy.io"},
      {"user-agent", "curl/7.54.1"}};
  Http::TestRequestHeaderMapImpl request_headers{headers};
  EXPECT_CALL(callbacks, requestHeaders())
      .WillRepeatedly(testing::Return(makeOptRef<RequestHeaderMap>(request_headers)));

  // Unsupported attributes.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter, envoy_dynamic_module_type_attribute_id_XdsListenerMetadata, &result_number));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_XdsListenerMetadata, &result_str_ptr,
      &result_str_length));

  // Type mismatch.
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter, envoy_dynamic_module_type_attribute_id_SourceAddress, &result_number));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_SourcePort, &result_str_ptr,
      &result_str_length));

  // envoy_dynamic_module_type_attribute_id_RequestProtocol
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestProtocol, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 8);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "HTTP/1.1");

  // envoy_dynamic_module_type_attribute_id_UpstreamAddress
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_UpstreamAddress, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 12);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "10.0.0.1:443");

  // envoy_dynamic_module_type_attribute_id_SourceAddress
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_SourceAddress, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 12);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "1.1.1.1:1234");

  // envoy_dynamic_module_type_attribute_id_DestinationAddress
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_DestinationAddress, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 14);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "127.0.0.2:4321");

  // envoy_dynamic_module_type_attribute_id_RequestId
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestId, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 36);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "ffffffff-0012-0110-00ff-0c00400600ff");

  // envoy_dynamic_module_type_attribute_id_XdsRouteName
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_XdsRouteName, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 10);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "test_route");

  // envoy_dynamic_module_type_attribute_id_RequestPath
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestPath, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 26);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "/api/v1/action?param=value");

  // envoy_dynamic_module_type_attribute_id_RequestHost
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestHost, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 11);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "example.org");

  // envoy_dynamic_module_type_attribute_id_RequestMethod
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestMethod, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 3);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "GET");

  // envoy_dynamic_module_type_attribute_id_RequestScheme
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestScheme, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 5);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "https");

  // envoy_dynamic_module_type_attribute_id_RequestUserAgent
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestUserAgent, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 11);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "curl/7.54.1");

  // envoy_dynamic_module_type_attribute_id_RequestReferer
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestReferer, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 13);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "envoyproxy.io");

  // envoy_dynamic_module_type_attribute_id_RequestQuery
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestQuery, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 11);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "param=value");

  // envoy_dynamic_module_type_attribute_id_RequestUrlPath
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestUrlPath, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 14);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "/api/v1/action");

  // test again without query params for envoy_dynamic_module_type_attribute_id_RequestUrlPath
  request_headers.setPath("/api/v1/action");
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_RequestUrlPath, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, 14);
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), "/api/v1/action");

  // empty connection, should return false
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter_without_callbacks, envoy_dynamic_module_type_attribute_id_ConnectionTlsVersion,
      &result_str_ptr, &result_str_length));

  EXPECT_CALL(callbacks, connection()).WillRepeatedly(testing::Return(absl::nullopt));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionUriSanPeerCertificate,
      &result_str_ptr, &result_str_length));

  // tests for TLS connection attributes
  const Network::MockConnection connection;
  EXPECT_CALL(callbacks, connection())
      .WillRepeatedly(
          testing::Return(makeOptRef(dynamic_cast<const Network::Connection&>(connection))));
  EXPECT_CALL(connection, ssl()).WillRepeatedly(testing::Return(nullptr));
  // no TLS, should return false
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionTlsVersion, &result_str_ptr,
      &result_str_length));

  // mock TLS and its attributes
  auto ssl = std::make_shared<Ssl::MockConnectionInfo>();
  EXPECT_CALL(connection, ssl()).WillRepeatedly(testing::Return(ssl));
  const std::string tls_version = "TLSv1.2";
  EXPECT_CALL(*ssl, tlsVersion()).WillRepeatedly(testing::ReturnRef(tls_version));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionTlsVersion, &result_str_ptr,
      &result_str_length));
  EXPECT_EQ(result_str_length, tls_version.size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), tls_version);

  const std::string digest = "sha256_digest";
  EXPECT_CALL(*ssl, sha256PeerCertificateDigest()).WillRepeatedly(testing::ReturnRef(digest));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionSha256PeerCertificateDigest,
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, digest.size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), digest);

  const std::string subject_cert = "subject_cert";
  EXPECT_CALL(*ssl, subjectLocalCertificate()).WillRepeatedly(testing::ReturnRef(subject_cert));
  EXPECT_CALL(*ssl, subjectPeerCertificate()).WillRepeatedly(testing::ReturnRef(subject_cert));

  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionSubjectLocalCertificate,
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, subject_cert.size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), subject_cert);

  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionSubjectPeerCertificate,
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, subject_cert.size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), subject_cert);

  // test returning the first entry when there are multiple SANs
  const std::vector<std::string> sans_empty;
  EXPECT_CALL(*ssl, dnsSansLocalCertificate()).WillOnce(testing::Return(sans_empty));
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionDnsSanLocalCertificate,
      &result_str_ptr, &result_str_length));

  const std::vector<std::string> sans = {"alt_name1", "alt_name2"};
  EXPECT_CALL(*ssl, dnsSansLocalCertificate()).WillRepeatedly(testing::Return(sans));
  EXPECT_CALL(*ssl, dnsSansPeerCertificate()).WillRepeatedly(testing::Return(sans));
  EXPECT_CALL(*ssl, uriSanLocalCertificate()).WillRepeatedly(testing::Return(sans));
  EXPECT_CALL(*ssl, uriSanPeerCertificate()).WillRepeatedly(testing::Return(sans));

  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionDnsSanLocalCertificate,
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, sans[0].size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), sans[0]);

  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionDnsSanPeerCertificate,
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, sans[0].size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), sans[0]);

  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionUriSanLocalCertificate,
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, sans[0].size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), sans[0]);

  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_string(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionUriSanPeerCertificate,
      &result_str_ptr, &result_str_length));
  EXPECT_EQ(result_str_length, sans[0].size());
  EXPECT_EQ(std::string(result_str_ptr, result_str_length), sans[0]);

  // envoy_dynamic_module_type_attribute_id_ResponseCode
  EXPECT_FALSE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter_without_callbacks, envoy_dynamic_module_type_attribute_id_ResponseCode,
      &result_number));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter, envoy_dynamic_module_type_attribute_id_ResponseCode, &result_number));
  EXPECT_EQ(result_number, 200);

  // envoy_dynamic_module_type_attribute_id_UpstreamPort
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter, envoy_dynamic_module_type_attribute_id_UpstreamPort, &result_number));
  EXPECT_EQ(result_number, 443);

  // envoy_dynamic_module_type_attribute_id_SourcePort
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter, envoy_dynamic_module_type_attribute_id_SourcePort, &result_number));
  EXPECT_EQ(result_number, 1234);

  // envoy_dynamic_module_type_attribute_id_DestinationPort
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter, envoy_dynamic_module_type_attribute_id_DestinationPort, &result_number));
  EXPECT_EQ(result_number, 4321);

  // envoy_dynamic_module_type_attribute_id_ConnectionId
  EXPECT_CALL(connection, id()).WillRepeatedly(testing::Return(8386));
  EXPECT_TRUE(envoy_dynamic_module_callback_http_filter_get_attribute_int(
      &filter, envoy_dynamic_module_type_attribute_id_ConnectionId, &result_number));
  EXPECT_EQ(result_number, 8386);
}

TEST(ABIImpl, HttpCallout) {
  DynamicModuleHttpFilter filter{nullptr};
  const std::string cluster{"some_cluster"};
  EXPECT_EQ(envoy_dynamic_module_callback_http_filter_http_callout(
                &filter, 1, const_cast<char*>(cluster.data()), cluster.size(), nullptr, 0, nullptr,
                0, 1000),
            envoy_dynamic_module_type_http_callout_init_result_MissingRequiredHeaders);
}

} // namespace HttpFilters
} // namespace DynamicModules
} // namespace Extensions
} // namespace Envoy
