# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: MIT

cmake_minimum_required (VERSION 3.13)
project(azure-security-keyvault-keys LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake-modules")

include(AzureVcpkg)
include(AzureVersion)
include(AzureCodeCoverage)
include(AzureTransportAdapters)
include(AzureDoxygen)
include(AzureGlobalCompileOptions)

az_vcpkg_integrate()

if(NOT AZ_ALL_LIBRARIES)
  find_package(azure-core-cpp "1.1.0" CONFIG QUIET)
  if(NOT azure-core-cpp_FOUND)
    find_package(azure-core-cpp "1.1.0" REQUIRED)
  endif()
endif()

set(
  AZURE_KEYVAULT_KEYS_HEADER
    inc/azure/keyvault/keys/cryptography/cryptography_client_options.hpp
    inc/azure/keyvault/keys/cryptography/cryptography_client.hpp
    inc/azure/keyvault/keys/cryptography/decrypt_parameters.hpp
    inc/azure/keyvault/keys/cryptography/decrypt_result.hpp
    inc/azure/keyvault/keys/cryptography/encrypt_parameters.hpp
    inc/azure/keyvault/keys/cryptography/encrypt_result.hpp
    inc/azure/keyvault/keys/cryptography/encryption_algorithm.hpp
    inc/azure/keyvault/keys/cryptography/key_wrap_algorithm.hpp
    inc/azure/keyvault/keys/cryptography/sign_result.hpp
    inc/azure/keyvault/keys/cryptography/signature_algorithm.hpp
    inc/azure/keyvault/keys/cryptography/wrap_result.hpp
    inc/azure/keyvault/keys/cryptography/unwrap_result.hpp
    inc/azure/keyvault/keys/cryptography/verify_result.hpp
    inc/azure/keyvault/keys/backup_key_result.hpp
    inc/azure/keyvault/keys/delete_key_operation.hpp
    inc/azure/keyvault/keys/deleted_key.hpp
    inc/azure/keyvault/keys/import_key_options.hpp
    inc/azure/keyvault/keys/json_web_key.hpp
    inc/azure/keyvault/keys/key_client.hpp
    inc/azure/keyvault/keys/key_create_options.hpp
    inc/azure/keyvault/keys/key_curve_name.hpp
    inc/azure/keyvault/keys/key_client_options.hpp
    inc/azure/keyvault/keys/key_operation.hpp
    inc/azure/keyvault/keys/key_properties.hpp
    inc/azure/keyvault/keys/key_type.hpp
    inc/azure/keyvault/keys/keyvault_key.hpp
    inc/azure/keyvault/keys/list_keys_responses.hpp
    inc/azure/keyvault/keys/recover_deleted_key_operation.hpp
)

set(
  AZURE_KEYVAULT_KEYS_SOURCE
    src/cryptography/cryptography_client_options.cpp
    src/cryptography/cryptography_client.cpp
    src/cryptography/decrypt_parameters.cpp
    src/cryptography/decrypt_result.cpp
    src/cryptography/encrypt_parameters.cpp
    src/cryptography/encrypt_result.cpp
    src/cryptography/encryption_algorithm.cpp
    src/cryptography/key_sign_parameters.cpp
    src/cryptography/key_verify_parameters.cpp
    src/cryptography/key_wrap_algorithm.cpp
    src/cryptography/key_wrap_parameters.cpp
    src/cryptography/remote_cryptography_client.cpp
    src/cryptography/rsa_cryptography_provider.cpp
    src/cryptography/sign_result.cpp
    src/cryptography/signature_algorithm.cpp
    src/cryptography/wrap_result.cpp
    src/cryptography/unwrap_result.cpp
    src/cryptography/verify_result.cpp
    src/private/cryptography_provider.hpp
    src/private/cryptography_serializers.hpp
    src/private/key_backup.hpp
    src/private/key_constants.hpp
    src/private/key_request_parameters.hpp
    src/private/key_serializers.hpp
    src/private/key_sign_parameters.hpp
    src/private/key_verify_parameters.hpp
    src/private/key_wrap_parameters.hpp
    src/private/keyvault_constants.hpp
    src/private/keyvault_protocol.hpp
    src/private/local_cryptography_provider.hpp
    src/private/local_cryptography_provider_factory.hpp
    src/private/package_version.hpp
    src/private/remote_cryptography_client.hpp
    src/private/rsa_cryptography_provider.hpp
    src/delete_key_operation.cpp
    src/deleted_key.cpp
    src/import_key_options.cpp
    src/json_web_key.cpp
    src/key_backup.cpp
    src/key_client_options.cpp
    src/key_client.cpp
    src/key_curve_name.cpp
    src/key_operation.cpp
    src/key_request_parameters.cpp
    src/key_type.cpp
    src/keyvault_key.cpp
    src/keyvault_protocol.cpp
    src/list_keys_responses.cpp
    src/recover_deleted_key_operation.cpp
)

add_library(azure-security-keyvault-keys
  ${AZURE_KEYVAULT_KEYS_HEADER} ${AZURE_KEYVAULT_KEYS_SOURCE}
)
add_library(Azure::azure-security-keyvault-keys ALIAS azure-security-keyvault-keys)

target_include_directories(
  azure-security-keyvault-keys
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
      $<INSTALL_INTERFACE:include>
)

target_link_libraries(azure-security-keyvault-keys PUBLIC Azure::azure-core)

# coverage. Has no effect if BUILD_CODE_COVERAGE is OFF
create_code_coverage(keyvault azure-security-keyvault-keys azure-security-keyvault-keys-test)

get_az_version("${CMAKE_CURRENT_SOURCE_DIR}/src/private/package_version.hpp")
generate_documentation(azure-security-keyvault-keys ${AZ_LIBRARY_VERSION})

if(BUILD_TESTING)

  if (NOT AZ_ALL_LIBRARIES)
    include(AddGoogleTest)
    enable_testing ()
  endif()
  
  add_subdirectory(test/ut)
endif()

if (BUILD_PERFORMANCE_TESTS)
  add_subdirectory(test/perf)
endif()

if(BUILD_SAMPLES)
  add_subdirectory(test/samples)
endif()

az_vcpkg_export(
    azure-security-keyvault-keys
    SECURITY_KEYVAULT_KEYS
    "azure/keyvault/keys/dll_import_export.hpp"
  )
