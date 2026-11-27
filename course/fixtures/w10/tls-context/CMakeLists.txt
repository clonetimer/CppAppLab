cmake_minimum_required(VERSION 3.18)
project(CppAppLabW10TlsContextAcceptance LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
find_package(GTest CONFIG REQUIRED)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS SSL Crypto)
find_program(OPENSSL_CLI NAMES openssl REQUIRED)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

# Generate disposable short-lived certificates in the build directory only.
# No private keys are stored in the repository or reused as credentials.
set(CERT_DIR "${CMAKE_CURRENT_BINARY_DIR}/local-certs")
file(MAKE_DIRECTORY "${CERT_DIR}")
function(local_openssl)
    execute_process(COMMAND "${OPENSSL_CLI}" ${ARGN}
        WORKING_DIRECTORY "${CERT_DIR}"
        RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE diagnostic TIMEOUT 15)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Local certificate fixture failed: ${result}: ${diagnostic}")
    endif()
endfunction()
local_openssl(req -new -x509 -newkey rsa:2048 -nodes -sha256 -days 2
    -subj "/CN=CppAppLab Local CA" -keyout ca.key -out ca.pem
    -addext "basicConstraints=critical,CA:TRUE"
    -addext "keyUsage=critical,keyCertSign,cRLSign")
local_openssl(req -new -x509 -newkey rsa:2048 -nodes -sha256 -days 2
    -subj "/CN=CppAppLab Unrelated CA" -keyout other.key -out other.pem
    -addext "basicConstraints=critical,CA:TRUE"
    -addext "keyUsage=critical,keyCertSign,cRLSign")
local_openssl(req -new -newkey rsa:2048 -nodes -sha256
    -subj "/CN=localhost" -keyout server.key -out server.csr)
file(WRITE "${CERT_DIR}/server.ext"
    "[leaf]\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n")
local_openssl(x509 -req -in server.csr -CA ca.pem -CAkey ca.key
    -CAcreateserial -days 2 -sha256 -extfile server.ext -extensions leaf -out server.pem)

add_library(tls_context_under_test STATIC
    "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/tls_context.cpp")
target_include_directories(tls_context_under_test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(tls_context_under_test PUBLIC OpenSSL::SSL OpenSSL::Crypto)
target_compile_options(tls_context_under_test PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
add_executable(tls_context_acceptance_tests tests/tls_context_test.cpp)
target_link_libraries(tls_context_acceptance_tests PRIVATE tls_context_under_test GTest::gtest_main)
target_compile_definitions(tls_context_acceptance_tests PRIVATE
    CPPAPPLAB_TLS_CERT_DIR="${CERT_DIR}")
target_compile_options(tls_context_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
include(GoogleTest)
gtest_discover_tests(tls_context_acceptance_tests PROPERTIES TIMEOUT 10)
