// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "azure/core/http/http.hpp"
#include "azure/core/http/winhttp/win_http_client.hpp"

#include <winhttp.h>
#include <Windows.h>
#include <iostream>
#include <string>

using namespace Azure::Core::Http;

std::wstring HttpMethodToWideString(HttpMethod method)
{
  // This string should be all uppercase.
  // Many servers treat HTTP verbs as case-sensitive, and the Internet Engineering Task Force (IETF)
  // Requests for Comments (RFCs) spell these verbs using uppercase characters only.
  switch (method)
  {
    case HttpMethod::Get:
      return L"GET";
    case HttpMethod::Head:
      return L"HEAD";
    case HttpMethod::Post:
      return L"POST";
    case HttpMethod::Put:
      return L"PUT";
    case HttpMethod::Delete:
      return L"DELETE";
    case HttpMethod::Patch:
      return L"PATCH";
    default:
      throw Azure::Core::Http::TransportException("Invalid HTTP method.");
  }
}

std::string WideStringToStringASCII(
    wchar_t const* const wideStringStart,
    wchar_t const* const wideStringEnd)
{
  // Converting this way is only safe when the text is ASCII.
#pragma warning(suppress : 4244)
  std::string str(wideStringStart, wideStringEnd);
  return str;
}

std::string WideStringToStringASCII(const std::wstring& wideString)
{
  // Converting this way is only safe when the text is ASCII.
#pragma warning(suppress : 4244)
  std::string str(wideString.begin(), wideString.end());
  return str;
}

// Convert a UTF-8 string to a wide Unicode string/.
std::wstring StringToWideString(const std::string& str)
{
  int strLength = static_cast<int>(str.size());
  int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), strLength, 0, 0);
  std::wstring wideStr(sizeNeeded, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), strLength, &wideStr[0], sizeNeeded);
  return wideStr;
}

// Convert a wide Unicode string to a UTF-8 string.
std::string WideStringToString(const std::wstring& wideString)
{
  int wideStrLength = static_cast<int>(wideString.size());
  int sizeNeeded
      = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), wideStrLength, NULL, 0, NULL, NULL);
  std::string str(sizeNeeded, 0);
  WideCharToMultiByte(
      CP_UTF8, 0, wideString.c_str(), wideStrLength, &str[0], sizeNeeded, NULL, NULL);
  return str;
}

static void ParseHttpVersion(
    std::string httpVersion,
    uint16_t* majorVersion,
    uint16_t* minorVersion)
{
  auto httpVersionEnd = httpVersion.data() + httpVersion.size();

  // set response code, http version and reason phrase (i.e. HTTP/1.1 200 OK)
  auto majorVersionStart
      = httpVersion.data() + 5; // HTTP = 4, / = 1, moving to 5th place for version
  auto majorVersionEnd = std::find(majorVersionStart, httpVersionEnd, '.');
  auto majorVersionInt = std::stoi(std::string(majorVersionStart, majorVersionEnd));

  auto minorVersionStart = majorVersionEnd + 1; // start of minor version
  auto minorVersionInt = std::stoi(std::string(minorVersionStart, httpVersionEnd));

  *majorVersion = (uint16_t)majorVersionInt;
  *minorVersion = (uint16_t)minorVersionInt;
}

std::unique_ptr<RawResponse> WinHttpTransport::Send(Context const& context, Request& request)
{
  // TODO: Propagate to allow request cancellation.
  (void)(context);

  // Use WinHttpOpen to obtain a session handle.
  // The dwFlags is set to 0 - all WinHTTP functions are performed synchronously.
  HINTERNET sessionHandle = WinHttpOpen(
      L"WinHTTP Example/1.0",
      WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0);

  if (!sessionHandle)
  {
    // ERROR_WINHTTP_INTERNAL_ERROR OR ERROR_NOT_ENOUGH_MEMORY
    throw Azure::Core::Http::TransportException(
        "Error while getting a session handle. Error Code: " + std::to_string(GetLastError()));
  }

  // TODO: Get port from Url
  // Specify an HTTP server.
  // Uses port 80 for HTTP and port 443 for HTTPS.
  // This function always operates synchronously.
  HINTERNET connectionHandle = WinHttpConnect(
      sessionHandle,
      StringToWideString(request.GetUrl().GetHost()).c_str(),
      INTERNET_DEFAULT_PORT,
      0);

  if (!connectionHandle)
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
        "Error while getting a connection handle. Error Code: " + std::to_string(error));
  }

  // Create an HTTP request handle.
  HINTERNET requestHandle = WinHttpOpenRequest(
      connectionHandle,
      HttpMethodToWideString(request.GetMethod()).c_str(),
      NULL, // Name of the target resource of the specified HTTP verb
      NULL, // Use HTTP/1.1
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, // No media types are accepted by the client
      WINHTTP_FLAG_SECURE); // Uses secure transaction semantics (SSL/TLS)

  if (!requestHandle)
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
        "Error while getting a request handle. Error Code: " + std::to_string(error));
  }

  // Send a request.
  BOOL sendResult = WinHttpSendRequest(
      requestHandle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

  if (!sendResult)
  {
    DWORD error = GetLastError();

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    // Errors include:
    // ERROR_WINHTTP_CANNOT_CONNECT
    // ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED
    // ERROR_WINHTTP_CONNECTION_ERROR
    // ERROR_WINHTTP_INCORRECT_HANDLE_STATE
    // ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
    // ERROR_WINHTTP_INTERNAL_ERROR
    // ERROR_WINHTTP_INVALID_URL
    // ERROR_WINHTTP_LOGIN_FAILURE
    // ERROR_WINHTTP_NAME_NOT_RESOLVED
    // ERROR_WINHTTP_OPERATION_CANCELLED
    // ERROR_WINHTTP_RESPONSE_DRAIN_OVERFLOW
    // ERROR_WINHTTP_SECURE_FAILURE
    // ERROR_WINHTTP_SHUTDOWN
    // ERROR_WINHTTP_TIMEOUT
    // ERROR_WINHTTP_UNRECOGNIZED_SCHEME
    // ERROR_NOT_ENOUGH_MEMORY
    // ERROR_INVALID_PARAMETER
    // ERROR_WINHTTP_RESEND_REQUEST
    throw Azure::Core::Http::TransportException(
        "Error while sending a request. Error Code: " + std::to_string(error));
  }

  // End the request.
  BOOL receiveResult = WinHttpReceiveResponse(requestHandle, NULL);

  if (!receiveResult)
  {
    DWORD error = GetLastError();

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    // Errors include:
    // ERROR_WINHTTP_CANNOT_CONNECT
    // ERROR_WINHTTP_CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW
    // ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED
    // ...
    // ERROR_WINHTTP_TIMEOUT
    // ERROR_WINHTTP_UNRECOGNIZED_SCHEME
    // ERROR_NOT_ENOUGH_MEMORY

    throw Azure::Core::Http::TransportException(
        "Error while receiving a response. Error Code: " + std::to_string(error));
  }

  DWORD sizeOfHeaders = 0;

  // First, use WinHttpQueryHeaders to obtain the size of the buffer.
  BOOL queryResult = WinHttpQueryHeaders(
      requestHandle,
      WINHTTP_QUERY_RAW_HEADERS_CRLF,
      WINHTTP_HEADER_NAME_BY_INDEX,
      NULL,
      &sizeOfHeaders,
      WINHTTP_NO_HEADER_INDEX);

  // The previous call is expected to fail since no destination buffer was provided.
  if (queryResult)
  {
    throw Azure::Core::Http::TransportException("Error while reading response headers.");
  }

  {
    DWORD error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER)
    {
      throw Azure::Core::Http::TransportException(
          "Error while reading response headers. Error Code: " + std::to_string(error));
    }
  }

  // Allocate memory for the buffer.
  std::vector<WCHAR> outputBuffer(sizeOfHeaders / sizeof(WCHAR), 0);

  if (!WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_VERSION,
          WINHTTP_HEADER_NAME_BY_INDEX,
          outputBuffer.data(),
          &sizeOfHeaders,
          WINHTTP_NO_HEADER_INDEX))
  {
    DWORD error = GetLastError();

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    throw Azure::Core::Http::TransportException(
        "Error while reading response headers. Error Code: " + std::to_string(error));
  }

  auto start = outputBuffer.data();
  std::string httpVersion = WideStringToStringASCII(start, start + sizeOfHeaders / sizeof(WCHAR));

  uint16_t majorVersion = 0;
  uint16_t minorVersion = 0;
  ParseHttpVersion(httpVersion, &majorVersion, &minorVersion);

  DWORD dwStatusCode = 0;
  DWORD dwSize = sizeof(dwStatusCode);

  if (!WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX,
          &dwStatusCode,
          &dwSize,
          WINHTTP_NO_HEADER_INDEX))
  {
    DWORD error = GetLastError();

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    throw Azure::Core::Http::TransportException(
        "Error while reading response headers. Error Code: " + std::to_string(error));
  }

  if (!WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_STATUS_TEXT,
          WINHTTP_HEADER_NAME_BY_INDEX,
          outputBuffer.data(),
          &sizeOfHeaders,
          WINHTTP_NO_HEADER_INDEX))
  {
    DWORD error = GetLastError();

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectionHandle);
    WinHttpCloseHandle(sessionHandle);

    throw Azure::Core::Http::TransportException(
        "Error while reading response headers. Error Code: " + std::to_string(error));
  }

  start = outputBuffer.data();
  std::string reasonPhrase
      = WideStringToString(std::wstring(start, start + sizeOfHeaders / sizeof(WCHAR)));

  // Allocate the instance of the response on the heap with a shared ptr so this memory gets
  // delegated outside the transport and will be eventually released.
  auto rawResponse = std::make_unique<RawResponse>(
      majorVersion, minorVersion, static_cast<HttpStatusCode>(dwStatusCode), reasonPhrase);

  // receiving a response

  // TODO: Call WinHttpQueryHeaders - after the receive response
  // - read the status header
  // -content type,
  //    content length
  //    - is it expected

  DWORD dwDownloaded = 0;
  LPSTR pszOutBuffer;

  // Keep checking for data until there is nothing left.
  do
  {
    // Check for available data.
    dwSize = 0;
    if (!WinHttpQueryDataAvailable(requestHandle, &dwSize))
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

      if (!WinHttpReadData(requestHandle, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded))
        printf("Error %u in WinHttpReadData.\n", GetLastError());
      else
        printf("%s\n", pszOutBuffer);

      // Free the memory allocated to the buffer.
      delete[] pszOutBuffer;
    }

  } while (dwSize > 0);

  // Close any open handles.
  WinHttpCloseHandle(requestHandle);
  WinHttpCloseHandle(connectionHandle);
  WinHttpCloseHandle(sessionHandle);

  return rawResponse;
}
