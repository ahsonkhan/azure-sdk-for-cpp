// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "../private/cryptography_serializers.hpp"
#include "../private/key_constants.hpp"
#include "azure/keyvault/keys/cryptography/cryptography_client_models.hpp"

#include <azure/core/base64.hpp>
#include <azure/core/internal/json/json.hpp>

#include <string>

using namespace Azure::Core::_internal;
using namespace Azure::Core::Json::_internal;
using namespace Azure::Security::KeyVault::Keys::_detail;

namespace Azure {
  namespace Security {
    namespace KeyVault {
      namespace Keys {
        namespace Cryptography {

  WrapResult _detail::WrapResultSerializer::WrapResultDeserialize(
      Azure::Core::Http::RawResponse const& rawResponse)
  {
    auto const& body = rawResponse.GetBody();
    auto jsonParser = json::parse(body);

    WrapResult result;
    result.KeyId = jsonParser[KeyIdPropertyName].get<std::string>();

    if (jsonParser.contains(ValueParameterValue) && !jsonParser[ValueParameterValue].is_null())
    {
      result.EncryptedKey
          = Base64Url::Base64UrlDecode(jsonParser[ValueParameterValue].get<std::string>());
    }

    return result;
  }

}}}}} // namespace Azure::Security::KeyVault::Keys::Cryptography
