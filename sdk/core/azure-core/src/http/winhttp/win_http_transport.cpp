// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "azure/core/http/http.hpp"
#include "azure/core/http/winhttp/win_http_client.hpp"

#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <winhttp.h>

using namespace Azure::Core::Http;

// WinHttpTransport::WinHttpTransport() : HttpTransport() {}
//
// WinHttpTransport::~WinHttpTransport() {}

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
      throw Azure::Core::Http::TransportException("Invalid or unsupported HTTP method.");
  }
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

std::string WideStringToStringASCII(
    wchar_t const* const wideStringStart,
    wchar_t const* const wideStringEnd)
{
  // Converting this way is only safe when the text is ASCII.
#pragma warning(suppress : 4244)
  std::string str(wideStringStart, wideStringEnd);
  return str;
}

void ParseHttpVersion(std::string httpVersion, uint16_t* majorVersion, uint16_t* minorVersion)
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

void ThrowAndCleanup(
    HINTERNET sessionHandle,
    HINTERNET connectionHandle,
    HINTERNET requestHandle,
    std::string exceptionMessage)
{
  if (requestHandle)
  {
    WinHttpCloseHandle(requestHandle);
  }

  if (connectionHandle)
  {
    WinHttpCloseHandle(connectionHandle);
  }

  if (sessionHandle)
  {
    WinHttpCloseHandle(sessionHandle);
  }

  throw Azure::Core::Http::TransportException(exceptionMessage);
}

void CleanupHandles(HINTERNET sessionHandle, HINTERNET connectionHandle, HINTERNET requestHandle)
{

  if (requestHandle)
  {
    WinHttpCloseHandle(requestHandle);
  }

  if (connectionHandle)
  {
    WinHttpCloseHandle(connectionHandle);
  }

  if (sessionHandle)
  {
    WinHttpCloseHandle(sessionHandle);
  }
}

void CleanupHandlesAndThrow(
    std::string exceptionMessage,
    HINTERNET sessionHandle = NULL,
    HINTERNET connectionHandle = NULL,
    HINTERNET requestHandle = NULL)
{
  DWORD error = GetLastError();

  CleanupHandles(sessionHandle, connectionHandle, requestHandle);

  throw Azure::Core::Http::TransportException(
      exceptionMessage + " Error Code: " + std::to_string(error) + ".");
}

std::unique_ptr<RawResponse> WinHttpTransport::Send(Context const& context, Request& request)
{
  // TODO: Need to test if context has been canceled and cleanup, can't call ThrowIfCanceled.
  (void)(context);

  // Use WinHttpOpen to obtain a session handle.
  // The dwFlags is set to 0 - all WinHTTP functions are performed synchronously.
  // TODO: Use specific user-agent or application name.
  HINTERNET sessionHandle = WinHttpOpen(
      L"WinHTTP Azure SDK",
      WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0);

  if (!sessionHandle)
  {
    // Errors include:
    // ERROR_WINHTTP_INTERNAL_ERROR
    // ERROR_NOT_ENOUGH_MEMORY
    CleanupHandlesAndThrow("Error while getting a session handle.");
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
    // Errors include:
    // ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
    // ERROR_WINHTTP_INTERNAL_ERROR
    // ERROR_WINHTTP_INVALID_URL
    // ERROR_WINHTTP_OPERATION_CANCELLED
    // ERROR_WINHTTP_UNRECOGNIZED_SCHEME
    // ERROR_WINHTTP_SHUTDOWN
    // ERROR_NOT_ENOUGH_MEMORY
    CleanupHandlesAndThrow("Error while getting a connection handle.", sessionHandle);
  }

  std::string path = request.GetUrl().GetPath();

  // Create an HTTP request handle.
  HINTERNET requestHandle = WinHttpOpenRequest(
      connectionHandle,
      HttpMethodToWideString(request.GetMethod()).c_str(),
      path.empty() ? NULL
                   : StringToWideString(request.GetUrl().GetPath())
                         .c_str(), // Name of the target resource of the specified HTTP verb
      NULL, // Use HTTP/1.1
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, // No media types are accepted by the client
      WINHTTP_FLAG_SECURE); // Uses secure transaction semantics (SSL/TLS)

  if (!requestHandle)
  {
    // Errors include:
    // ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
    // ERROR_WINHTTP_INTERNAL_ERROR
    // ERROR_WINHTTP_INVALID_URL
    // ERROR_WINHTTP_OPERATION_CANCELLED
    // ERROR_WINHTTP_UNRECOGNIZED_SCHEME
    // ERROR_NOT_ENOUGH_MEMORY
    CleanupHandlesAndThrow(
        "Error while getting a request handle.", sessionHandle, connectionHandle);
  }

  std::wstring encodedHeaders;
  int encodedHeadersLength = 0;

  // TODO: Consider saving this to a field to avoid multiple request processing on retry.
  auto requestHeaders = request.GetHeaders();
  std::string requestHeaderString;
  if (requestHeaders.size() != 0)
  {
    // The encodedHeaders will be null-terminated and the length is calculated.
    encodedHeadersLength = -1;
    for (auto const& pairs : requestHeaders)
    {
      requestHeaderString.append(pairs.first); // string (key)
      requestHeaderString.append(": ");
      requestHeaderString.append(pairs.second); // string's value
      requestHeaderString.append("\r\n");
    }
    requestHeaderString.append("\0");

    encodedHeaders = StringToWideString(requestHeaderString);
  }

  // Send a request.
  // TODO: For PUT/POST requests, send additional data using WinHttpWriteData.
  // TODO: Support chunked transfer encoding and missing content-length header.
  if (!WinHttpSendRequest(
          requestHandle,
          requestHeaders.size() == 0 ? WINHTTP_NO_ADDITIONAL_HEADERS : encodedHeaders.c_str(),
          encodedHeadersLength,
          WINHTTP_NO_REQUEST_DATA,
          0,
          0,
          0))
  {
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
    CleanupHandlesAndThrow(
        "Error while sending a request.", sessionHandle, connectionHandle, requestHandle);
  }

  // End the request.
  if (!WinHttpReceiveResponse(requestHandle, NULL))
  {
    // Errors include:
    // ERROR_WINHTTP_CANNOT_CONNECT
    // ERROR_WINHTTP_CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW
    // ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED
    // ...
    // ERROR_WINHTTP_TIMEOUT
    // ERROR_WINHTTP_UNRECOGNIZED_SCHEME
    // ERROR_NOT_ENOUGH_MEMORY
    CleanupHandlesAndThrow(
        "Error while receiving a response.", sessionHandle, connectionHandle, requestHandle);
  }

  // First, use WinHttpQueryHeaders to obtain the size of the buffer.
  // The call is expected to fail since no destination buffer is provided.
  DWORD sizeOfHeaders = 0;
  if (WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_RAW_HEADERS_CRLF,
          WINHTTP_HEADER_NAME_BY_INDEX,
          NULL,
          &sizeOfHeaders,
          WINHTTP_NO_HEADER_INDEX))
  {
    CleanupHandles(sessionHandle, connectionHandle, requestHandle);
    throw Azure::Core::Http::TransportException("Error while querying response headers.");
  }

  {
    DWORD error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER)
    {
      CleanupHandles(sessionHandle, connectionHandle, requestHandle);
      throw Azure::Core::Http::TransportException(
          "Error while querying response headers. Error Code: " + std::to_string(error) + ".");
    }
  }

  // Allocate memory for the buffer.
  std::vector<WCHAR> outputBuffer(sizeOfHeaders / sizeof(WCHAR), 0);

  // Now, use WinHttpQueryHeaders to retrieve all the headers.
  if (!WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_RAW_HEADERS_CRLF,
          WINHTTP_HEADER_NAME_BY_INDEX,
          outputBuffer.data(),
          &sizeOfHeaders,
          WINHTTP_NO_HEADER_INDEX))
  {
    CleanupHandlesAndThrow(
        "Error while querying response headers.", sessionHandle, connectionHandle, requestHandle);
  }

  auto start = outputBuffer.data();
  auto last = start + sizeOfHeaders / sizeof(WCHAR);
  auto statusLineEnd = std::find(start, last, '\n');
  start = statusLineEnd + 1; // start of headers
  std::string responseHeaders = WideStringToString(std::wstring(start, last - (statusLineEnd + 1)));

  // Get the HTTP version.
  if (!WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_VERSION,
          WINHTTP_HEADER_NAME_BY_INDEX,
          outputBuffer.data(),
          &sizeOfHeaders,
          WINHTTP_NO_HEADER_INDEX))
  {
    CleanupHandlesAndThrow(
        "Error while querying response headers.", sessionHandle, connectionHandle, requestHandle);
  }

  start = outputBuffer.data();
  std::string httpVersion = WideStringToStringASCII(start, start + sizeOfHeaders / sizeof(WCHAR));

  uint16_t majorVersion = 0;
  uint16_t minorVersion = 0;
  ParseHttpVersion(httpVersion, &majorVersion, &minorVersion);

  DWORD statusCode = 0;
  DWORD dwSize = sizeof(statusCode);

  // Get the status code as a number.
  if (!WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX,
          &statusCode,
          &dwSize,
          WINHTTP_NO_HEADER_INDEX))
  {
    CleanupHandlesAndThrow(
        "Error while querying response headers.", sessionHandle, connectionHandle, requestHandle);
  }

  HttpStatusCode httpStatusCode = static_cast<HttpStatusCode>(statusCode);

  // Get the optional reason phrase.
  std::string reasonPhrase;

  if (WinHttpQueryHeaders(
          requestHandle,
          WINHTTP_QUERY_STATUS_TEXT,
          WINHTTP_HEADER_NAME_BY_INDEX,
          outputBuffer.data(),
          &sizeOfHeaders,
          WINHTTP_NO_HEADER_INDEX))
  {
    start = outputBuffer.data();
    reasonPhrase = WideStringToString(std::wstring(start, start + sizeOfHeaders / sizeof(WCHAR)));
  }

  DWORD dwContentLength = 0;
  dwSize = sizeof(dwContentLength);

  int64_t contentLength = 0;

  // Get the content length as a number.
  if (httpStatusCode != HttpStatusCode::NoContent)
  {
    if (!WinHttpQueryHeaders(
            requestHandle,
            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &dwContentLength,
            &dwSize,
            WINHTTP_NO_HEADER_INDEX))
    {
      contentLength = -1;

      // if (!WinHttpQueryHeaders(
      //        requestHandle,
      //        WINHTTP_QUERY_TRANSFER_ENCODING,
      //        WINHTTP_HEADER_NAME_BY_INDEX,
      //        outputBuffer.data(),
      //        &sizeOfHeaders,
      //        WINHTTP_NO_HEADER_INDEX))
      //{

      //  WinHttpQueryHeaders(
      //      requestHandle,
      //      WINHTTP_QUERY_CONTENT_ENCODING,
      //      WINHTTP_HEADER_NAME_BY_INDEX,
      //      outputBuffer.data(),
      //      &sizeOfHeaders,
      //      WINHTTP_NO_HEADER_INDEX);

      //  WinHttpQueryHeaders(
      //      requestHandle,
      //      WINHTTP_QUERY_CONTENT_TRANSFER_ENCODING,
      //      WINHTTP_HEADER_NAME_BY_INDEX,
      //      outputBuffer.data(),
      //      &sizeOfHeaders,
      //      WINHTTP_NO_HEADER_INDEX);

      //  CleanupHandlesAndThrow(
      //      "Error while querying response headers.", sessionHandle, connectionHandle,
      //      requestHandle);
      //}

      // start = outputBuffer.data();
      // std::string transferEncoding
      //    = WideStringToStringASCII(start, start + sizeOfHeaders / sizeof(WCHAR));

      // if (transferEncoding != "chunked")
      //{
      //  CleanupHandlesAndThrow(
      //      "Error while querying response headers.", sessionHandle, connectionHandle,
      //      requestHandle);
      //}
    }
    else
    {
      contentLength = static_cast<int64_t>(dwContentLength);
    }
  }

  auto stream = std::make_unique<Details::WinHttpStream>(
      sessionHandle, connectionHandle, requestHandle, contentLength);

  // Allocate the instance of the response on the heap with a shared ptr so this memory gets
  // delegated outside the transport and will be eventually released.
  auto rawResponse
      = std::make_unique<RawResponse>(majorVersion, minorVersion, httpStatusCode, reasonPhrase);

  rawResponse->SetBodyStream(std::move(stream));
  rawResponse->AddHeaders(responseHeaders);

  return rawResponse;
}

// Read from curl session
int64_t Details::WinHttpStream::Read(Context const& context, uint8_t* buffer, int64_t count)
{
  context.ThrowIfCanceled();

  if (count <= 0 || this->m_isEOF)
  {
    return 0;
  }

  int64_t totalNumberOfBytesRead = 0;
  DWORD numberOfBytesRead = 0;

  // Keep checking for data until there is nothing left.
  do
  {
    context.ThrowIfCanceled();

    // Check for available data.
    DWORD numberOfBytesAvailable = 0;
    if (!WinHttpQueryDataAvailable(this->m_requestHandle, &numberOfBytesAvailable))
    {
      // Errors include:
      // ERROR_WINHTTP_CONNECTION_ERROR
      // ERROR_WINHTTP_INCORRECT_HANDLE_STATE
      // ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
      // ERROR_WINHTTP_INTERNAL_ERROR
      // ERROR_WINHTTP_OPERATION_CANCELLED
      // ERROR_WINHTTP_TIMEOUT
      // ERROR_NOT_ENOUGH_MEMORY

      DWORD error = GetLastError();
      throw Azure::Core::Http::TransportException(
          "Error while querying how much data is available to read. Error Code: "
          + std::to_string(error) + ".");
    }

    context.ThrowIfCanceled();

    DWORD numberOfBytesToRead = numberOfBytesAvailable;
    if (numberOfBytesAvailable > count)
    {
      numberOfBytesToRead = static_cast<DWORD>(count);
    }

    if (!WinHttpReadData(
            this->m_requestHandle,
            (LPVOID)(buffer + totalNumberOfBytesRead),
            numberOfBytesToRead,
            &numberOfBytesRead))
    {
      // Errors include:
      // ERROR_WINHTTP_CONNECTION_ERROR
      // ERROR_WINHTTP_INCORRECT_HANDLE_STATE
      // ERROR_WINHTTP_INCORRECT_HANDLE_TYPE
      // ERROR_WINHTTP_INTERNAL_ERROR
      // ERROR_WINHTTP_OPERATION_CANCELLED
      // ERROR_WINHTTP_RESPONSE_DRAIN_OVERFLOW
      // ERROR_WINHTTP_TIMEOUT
      // ERROR_NOT_ENOUGH_MEMORY

      DWORD error = GetLastError();
      throw Azure::Core::Http::TransportException(
          "Error while querying how much data is available to read. Error Code: "
          + std::to_string(error) + ".");
    }

    totalNumberOfBytesRead += numberOfBytesRead;
    count -= numberOfBytesRead;

    if (numberOfBytesRead == 0
        || (this->m_contentLength != -1
            && this->m_streamTotalRead == this->m_contentLength - totalNumberOfBytesRead))
    {
      this->m_isEOF = true;
      break;
    }

  } while (count > 0);

  this->m_streamTotalRead += totalNumberOfBytesRead;
  return totalNumberOfBytesRead;
}