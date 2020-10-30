// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "azure/core/http/http.hpp"
#include "azure/core/http/winhttp/win_http_client.hpp"

#include <Winhttp.h>
#include <string>
#include <windows.h>

using namespace Azure::Core::Http;

WinHttpTansport::WinHttpTansport() : HttpTransport() {}

WinHttpTansport::~WinHttpTansport() {}

std::string HttpMethodName(HttpMethod method)
{
  // This string should be all uppercase.
  // Many servers treat HTTP verbs as case-sensitive, and the Internet Engineering Task Force (IETF)
  // Requests for Comments (RFCs) spell these verbs using uppercase characters only.
  switch (method)
  {
    case Get:
      return "GET";
    case Head:
      return "HEAD";
    case Post:
      return "POST";
    case Put:
      return "PUT";
    case Delete:
      return "DELETE";
    case Patch:
      return "PATCH";
    default:
      throw Azure::Core::Http::TransportException("Invalid HTTP method.");
  }
}

std::unique_ptr<RawResponse> WinHttpTansport::Send(Context const& context, Request& request)
{
  void(context);

  DWORD dwSize = 0;
  DWORD dwDownloaded = 0;
  LPSTR pszOutBuffer;
  BOOL sendResult = FALSE;
  HINTERNET connectionHandle = NULL, requestHandle = NULL;

  // Use WinHttpOpen to obtain a session handle.
  // The dwFlags is set to 0 - all WinHTTP functions are performed synchronously.
  HINTERNET sessionHandle = WinHttpOpen(
      L"WinHTTP Example/1.0",
      WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0);

  // TODO: Get port from Url
  // Specify an HTTP server.
  if (sessionHandle)
  {
    // Uses port 80 for HTTP and port 443 for HTTPS.
    // This function always operates synchronously.
    connectionHandle = WinHttpConnect(
        sessionHandle, request.GetUrl().GetHost().c_str(), INTERNET_DEFAULT_PORT, 0);
  }
  else
  {
    // ERROR_WINHTTP_INTERNAL_ERROR OR ERROR_NOT_ENOUGH_MEMORY
    throw Azure::Core::Http::TransportException(
        "Error while sending request. Error Code: " + std::to_string(GetLastError()));
  }

  // Create an HTTP request handle.
  if (connectionHandle)
  {
    requestHandle = WinHttpOpenRequest(
        connectionHandle,
        HttpMethodName(request.GetMethod()).c_str(),
        NULL,
        NULL, // Use HTTP/1.1
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, // No media types are accepted by the client
        WINHTTP_FLAG_SECURE); // Uses secure transaction semantics (SSL/TLS)
  }
  else
  {
    DWORD error = GetLastError();

    WinHttpCloseHandle(sessionHandle);

    // Errors include:
    // ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
    // ERROR_WINHTTP_INTERNAL_ERROR
    // ERROR_WINHTTP_INVALID_URL
    // ERROR_WINHTTP_OPERATION_CANCELLED
    // ERROR_WINHTTP_UNRECOGNIZED_SCHEME
    // ERROR_WINHTTP_SHUTDOWN
    // ERROR_NOT_ENOUGH_MEMORY
    throw Azure::Core::Http::TransportException(
        "Error while sending request. Error Code: " + std::to_string(error));
  }

  // Send a request.
  if (requestHandle)
  {
    sendResult = WinHttpSendRequest(
        requestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  }
  else
  {
    DWORD error = GetLastError();

    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    // Errors include:
    // ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
    // ERROR_WINHTTP_INTERNAL_ERROR
    // ERROR_WINHTTP_INVALID_URL
    // ERROR_WINHTTP_OPERATION_CANCELLED
    // ERROR_WINHTTP_UNRECOGNIZED_SCHEME
    // ERROR_NOT_ENOUGH_MEMORY
    throw Azure::Core::Http::TransportException(
        "Error while sending request. Error Code: " + std::to_string(error));
  }

  // End the request.
  if (sendResult)
    bResults = WinHttpReceiveResponse(hRequest, NULL);

  // TODO: Call WinHttpQueryHeaders - after the receive response
  // - read the status header
  // -content type,
  //    content length
  //    - is it expected

  // Keep checking for data until there is nothing left.
  if (bResults)
    do
    {
      // Check for available data.
      dwSize = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
        printf("Error %u in WinHttpQueryDataAvailable.\n", GetLastError());

      // Allocate space for the buffer.
      pszOutBuffer = new char[dwSize + 1];
      if (!pszOutBuffer)
      {
        printf("Out of memory\n");
        dwSize = 0;
      }
      else
      {
        // Read the Data.
        ZeroMemory(pszOutBuffer, dwSize + 1);

        if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
          printf("Error %u in WinHttpReadData.\n", GetLastError());
        else
          printf("%s\n", pszOutBuffer);

        // Free the memory allocated to the buffer.
        delete[] pszOutBuffer;
      }

    } while (dwSize > 0);

  // Report any errors.
  if (!bResults)
    printf("Error %d has occurred.\n", GetLastError());

  // Close any open handles.
  if (hRequest)
    WinHttpCloseHandle(hRequest);
  if (hConnect)
    WinHttpCloseHandle(hConnect);
  if (hSession)
    WinHttpCloseHandle(hSession);

  throw;
}
