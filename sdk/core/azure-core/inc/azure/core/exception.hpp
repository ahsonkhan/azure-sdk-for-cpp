// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

/**
 * @file
 * @brief Define RequestFailedException. It is used by HTTP exceptions.
 */

#pragma once

#include <stdexcept>

namespace Azure { namespace Core {
  /**
   * @brief An error while trying to send a request to Azure service.
   *
   */
  class RequestFailedException : public std::runtime_error {
  public:
    /**
     * @brief Construct a new Request Failed Exception object.
     *
     * @param message The error description.
     */
    explicit RequestFailedException(std::string const& message) : std::runtime_error(message) {}
    // Sanitize headers similar to logging allow-list (headers and query parameters)
    // Including: response headers
    // Content: Log content configurable toggle - exception shows the content in .NET

  };
}} // namespace Azure::Core
