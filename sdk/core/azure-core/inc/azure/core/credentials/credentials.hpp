// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

/**
 * @file
 * @brief Credentials used for authentication with many (not all) Azure SDK client libraries.
 */

#pragma once

#include "azure/core/context.hpp"
#include "azure/core/datetime.hpp"

#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Azure { namespace Core { namespace Credentials {

  /**
   * @brief An access token is used to authenticate requests.
   */
  struct AccessToken final
  {
    /**
     * @brief Token string.
     *
     */
    std::string Token;

    /**
     * @brief A point in time after which the token expires.
     *
     */
    DateTime ExpiresOn;
  };

  /**
   * @brief Context for getting token.
   */
  struct TokenRequestContext final
  {
    /**
     * @brief Authentication scopes.
     *
     */
    std::vector<std::string> Scopes;

    /**
     * @brief Minimum token expiration suggestion.
     *
     */
    DateTime::duration MinimumExpiration = std::chrono::minutes(2);

    /**
     * @brief Tenant ID.
     *
     */
    std::string TenantId;

    /*
     * @brief Additional claims to be included in the token to satisfy a conditional access policy,
     * such as a service may return in a claims challenge following an authorization failure. See
     * https://openid.net/specs/openid-connect-core-1_0-final.html#ClaimsParameter for more
     * information on format and content.
     */
    Nullable<std::string> Claims;

    /*
     * @brief Indicates whether to enable Continuous Access Evaluation (CAE) for the requested
     * token.
     *
     * @remark If a resource API implements CAE and your application declares it can handle CAE,
     * your app receives CAE tokens for that resource. For this reason, if you declare your app CAE
     * ready, your application must handle the CAE claim challenge for all resource APIs that accept
     * Microsoft Identity access tokens. If you don't handle CAE responses in these API calls, your
     * app could end up in a loop retrying an API call with a token that is still in the returned
     * lifespan of the token but has been revoked due to CAE.
     */
    bool IsCaeEnabled;
  };

  /**
   * @brief A base type of credential that uses Azure::Core::AccessToken to authenticate requests.
   */
  class TokenCredential {
  private:
    std::string m_credentialName;

  public:
    /**
     * @brief Gets an authentication token.
     *
     * @param tokenRequestContext A context to get the token in.
     * @param context A context to control the request lifetime.
     *
     * @return Authentication token.
     *
     * @throw Azure::Core::Credentials::AuthenticationException Authentication error occurred.
     */
    virtual AccessToken GetToken(
        TokenRequestContext const& tokenRequestContext,
        Context const& context) const = 0;

    /**
     * @brief Gets the name of the credential.
     *
     */
    std::string const& GetCredentialName() const { return m_credentialName; }

    /**
     * @brief Destructs `%TokenCredential`.
     *
     */
    virtual ~TokenCredential() = default;

  protected:
    /**
     * @brief Constructs an instance of `%TokenCredential`.
     *
     * @param credentialName Name of the credential for diagnostic messages.
     */
    TokenCredential(std::string const& credentialName)
        : m_credentialName(credentialName.empty() ? "Custom Credential" : credentialName)
    {
    }

    /**
     * @brief Constructs a default instance of `%TokenCredential`.
     *
     * @deprecated Use the constructor with parameter.
     */
    [[deprecated("Use the constructor with parameter.")]] TokenCredential()
        : TokenCredential(std::string{})
    {
    }

  private:
    /**
     * @brief `%TokenCredential` does not allow copy construction.
     *
     */
    TokenCredential(TokenCredential const&) = delete;

    /**
     * @brief `%TokenCredential` does not allow assignment.
     *
     */
    void operator=(TokenCredential const&) = delete;
  };

  /**
   * @brief An exception that gets thrown when an authentication error occurs.
   */
  class AuthenticationException final : public std::exception {
    std::string m_what;

  public:
    /**
     * @brief Constructs `%AuthenticationException` with a message string.
     *
     * @param what The explanatory string.
     */
    explicit AuthenticationException(std::string what) : m_what(std::move(what)) {}

    /**
     * Gets the explanatory string.
     *
     * @note See https://en.cppreference.com/w/cpp/error/exception/what.
     *
     * @return C string with explanatory information.
     */
    char const* what() const noexcept override { return m_what.c_str(); }
  };
}}} // namespace Azure::Core::Credentials
