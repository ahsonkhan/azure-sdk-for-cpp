// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

/**
 * @file
 * @brief A remote client used to perform cryptographic operations with Azure Key Vault keys.
 *
 */

#pragma once

#include <azure/core/response.hpp>
#include <azure/core/url.hpp>

#include "cryptography_provider.hpp"
#include "keyvault_protocol.hpp"

#include "azure/keyvault/keys/cryptography/cryptography_client_options.hpp"
#include "azure/keyvault/keys/cryptography/encrypt_parameters.hpp"
#include "azure/keyvault/keys/cryptography/encrypt_result.hpp"
#include "azure/keyvault/keys/keyvault_key.hpp"

#include <memory>
#include <string>

namespace Azure {
  namespace Security {
    namespace KeyVault {
      namespace Keys {
        namespace Cryptography {
  namespace _detail {

    class RemoteCryptographyClient final
        : public Azure::Security::KeyVault::Keys::Cryptography::_detail::CryptographyProvider {
    public:
      std::shared_ptr<Azure::Security::KeyVault::_detail::KeyVaultProtocolClient> Pipeline;
      Azure::Core::Url KeyId;

      explicit RemoteCryptographyClient(
          std::string const& keyId,
          std::shared_ptr<Core::Credentials::TokenCredential const> credential,
          CryptographyClientOptions options = CryptographyClientOptions());

      bool CanRemote() const noexcept override { return true; }

      bool SupportsOperation(Azure::Security::KeyVault::Keys::KeyOperation) const noexcept override
      {
        return true;
      };

      Azure::Response<KeyVaultKey> GetKey(
          Azure::Core::Context const& context = Azure::Core::Context()) const;

      EncryptResult Encrypt(
          EncryptParameters const& parameters,
          Azure::Core::Context const& context) const override;

      DecryptResult Decrypt(
          DecryptParameters const& parameters,
          Azure::Core::Context const& context) const override;

      WrapResult WrapKey(
          KeyWrapAlgorithm const& algorithm,
          std::vector<uint8_t> const& key,
          Azure::Core::Context const& context) const override;

      UnwrapResult UnwrapKey(
          KeyWrapAlgorithm const& algorithm,
          std::vector<uint8_t> const& encryptedKey,
          Azure::Core::Context const& context) const override;

      SignResult Sign(
          SignatureAlgorithm const& algorithm,
          std::vector<uint8_t> const& digest,
          Azure::Core::Context const& context) const override;

      VerifyResult Verify(
          SignatureAlgorithm const& algorithm,
          std::vector<uint8_t> const& digest,
          std::vector<uint8_t> const& signature,
          Azure::Core::Context const& context) const override;
    };
}}}}}} // namespace Azure::Security::KeyVault::Keys::Cryptography::_detail
