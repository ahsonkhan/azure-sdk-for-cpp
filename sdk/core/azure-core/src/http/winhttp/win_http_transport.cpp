// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "azure/core/http/http.hpp"
#include "azure/core/http/winhttp/win_http_client.hpp"

#include <string>
#include <Winhttp.h>

using namespace Azure::Core::Http;

WinHttpTansport::WinHttpTansport() : HttpTransport() {}

WinHttpTansport::~WinHttpTansport() {}

std::unique_ptr<RawResponse> WinHttpTansport::Send(Context const& context, Request& request)
{
  void(context);
  void(request);

  DWORD dwSize = 0;
  DWORD dwDownloaded = 0;
  LPSTR pszOutBuffer;
  BOOL bResults = FALSE;
  HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

  // Use WinHttpOpen to obtain a session handle.
  // The dwFlags is set to 0 - all WinHTTP functions are performed synchronously.
  hSession = WinHttpOpen(
      L"WinHTTP Example/1.0",
      WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,
      0);

  // TODO: Get port and hostname from Url
  // Specify an HTTP server.
  if (hSession)
    hConnect = WinHttpConnect(hSession, L"www.microsoft.com", INTERNET_DEFAULT_PORT, 0);

  // Create an HTTP request handle.
  if (hConnect)
    hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        NULL,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

  // Send a request.
  if (hRequest)
    bResults = WinHttpSendRequest(
        hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

  // End the request.
  if (bResults)
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

