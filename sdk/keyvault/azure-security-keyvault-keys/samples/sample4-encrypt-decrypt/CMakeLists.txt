# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.13)

project (sample4-encrypt-decrypt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable (
  sample4-encrypt-decrypt
  sample4_encrypt_decrypt.cpp
)

target_compile_definitions(sample4-encrypt-decrypt PRIVATE _azure_BUILDING_SAMPLES)

create_per_service_target_build_for_sample(keyvault sample4-encrypt-decrypt)

target_link_libraries(sample4-encrypt-decrypt PRIVATE azure-security-keyvault-keys azure-identity get-env-helper)
