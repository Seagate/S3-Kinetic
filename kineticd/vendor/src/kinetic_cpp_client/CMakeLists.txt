cmake_minimum_required(VERSION 2.8.6)
project(kinetic_cpp_client CXX C)

find_package (Threads)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_FOR_ARM "Build for ARM instead of x86" off)
option(BUILD_PIC "Build static PIC" off)
option(BUILD_FOR_ARM64 "Build for 64 bit arm processor" off)

message("Checking for input TOOLCHAIN FILE")
message("kinetic_cpp_client CMakeLists Input TOOLCHAIN FILE: ${CMAKE_TOOLCHAIN_FILE}")

set(BUILD_PIC_COMPILER_FLAGS "")

if(BUILD_PIC)
  set(BUILD_PIC_COMPILER_FLAGS "-fPIC")
endif(BUILD_PIC)

message("Incoming CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${PARENT_CXX_FLAGS}")
message("Configured CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

if(${BUILD_FOR_ARM})
  set(CMAKE_C_COMPILER $ENV{C_COMPILER})
  set(CMAKE_CXX_COMPILER $ENV{CXX_COMPILER})
  set(CMAKE_RANLIB $ENV{C_RANLIB})
  set(CHILD_MAKE_FLAGS CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} RANLIB=${CMAKE_RANLIB})
else(${BUILD_FOR_ARM})
  set(CMAKE_C_COMPILER "gcc")
  set(CMAKE_CXX_COMPILER "g++")
endif(${BUILD_FOR_ARM})

set(CMAKE_ARCH_LIB_PATH $ENV{C_ARCH_LIB_PATH})
set(CMAKE_ARCH_BIN_PATH $ENV{C_ARCH_BIN_PATH})

set(TEST_BINARY "kinetic_client_test")
set(TEST_BINARY_PATH ${kinetic_cpp_client_BINARY_DIR}/${TEST_BINARY})
set(INTEGRATION_TEST_BINARY "kinetic_integration_test")
set(INTEGRATION_TEST_BINARY_PATH ${kinetic_cpp_client_BINARY_DIR}/${INTEGRATION_TEST_BINARY})
set(TEST_LIBRARIES
    glog
    gtest
    gmock
    OpenSSL::Crypto
    OpenSSL::SSL
)

set(GENERATED_SOURCES_PATH ${kinetic_cpp_client_SOURCE_DIR}/src/main/generated)

set(PREFIX "${CMAKE_BINARY_DIR}/vendor")
set(EXTERNAL_PREFIX "${kinetic_cpp_client_BINARY_DIR}/vendor")
set(CMAKE_INSTALL_PREFIX ${EXTERNAL_PREFIX})

message("******PREFIX:    ${CMAKE_BINARY_DIR}/vendor")
message("******EXTERNAL_PREFIX:  ${kinetic_cpp_client_BINARY_DIR}/vendor")

include(ExternalProject)

message("Configuring static build targets")
SET(BUILD_SHARED_LIBS OFF)
SET(CMAKE_EXE_LINKER_FLAGS "-static")

add_library(gflags STATIC IMPORTED)
set_property(TARGET gflags PROPERTY IMPORTED_LOCATION ${CMAKE_ARCH_LIB_PATH}/lib/libgflags.a)

add_library(glog STATIC IMPORTED)
set_property(TARGET glog PROPERTY IMPORTED_LOCATION ${CMAKE_ARCH_LIB_PATH}/lib/libglog.a)

add_library(gtest STATIC IMPORTED)
set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${CMAKE_ARCH_LIB_PATH}/lib/libgtest.a)

add_library(gmock STATIC IMPORTED)
set_property(TARGET gmock PROPERTY IMPORTED_LOCATION ${CMAKE_ARCH_LIB_PATH}/lib/libgmock.a)


set(OPENSSL_USE_STATIC_LIBS TRUE)
set(OPENSSL_ROOT_DIR ${CMAKE_ARCH_LIB_PATH})
find_package(OpenSSL REQUIRED)

# The protobuf build requires the existence of a protoc binary that can be
# executed on the host machine. To handle cross compilation, we always build
# protobuf once for the host so that we have a suitable copy of protoc.
add_library(protoc STATIC IMPORTED)
set_property(TARGET protoc PROPERTY IMPORTED_LOCATION ${CMAKE_ARCH_LIB_PATH}/lib/libprotoc.a)

# Protobuf code generation rules
set(PROTOC_PATH "${CMAKE_ARCH_BIN_PATH}/bin/protoc")
#set(PROTO_DIR "${CMAKE_BINARY_DIR}/vendor/src/kinetic-proto")
set(PROTO_ORIG_PATH "${PROTO_DIR}/kinetic.proto")
set(PROTO_MODIFIED_PATH "${PROTO_DIR}/kinetic_client.proto")
add_library(protobuf STATIC IMPORTED)
set_property(TARGET protobuf PROPERTY IMPORTED_LOCATION ${CMAKE_ARCH_LIB_PATH}/lib/libprotobuf.a)

add_custom_command(
    COMMENT "Compiling protobuf"
    OUTPUT ${GENERATED_SOURCES_PATH}/kinetic_client.pb.h ${GENERATED_SOURCES_PATH}/kinetic_client.pb.cc
    COMMAND mkdir -p ${GENERATED_SOURCES_PATH} && sed 's/com\\.seagate\\.kinetic\\.proto/com.seagate.kinetic.client.proto/' ${PROTO_ORIG_PATH} > ${PROTO_MODIFIED_PATH} && ${PROTOC_PATH} -I=${PROTO_DIR} --cpp_out=${GENERATED_SOURCES_PATH} ${PROTO_MODIFIED_PATH}
    DEPENDS protoc protobuf
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${GENERATED_SOURCES_PATH})

include_directories(
    include
    src/main/generated
    src/main
    src/test/mock
    src/test
    ${KERNEL_MEM_DIR}/
    ${CMAKE_BINARY_DIR}/smrdb/include
    #${EXTERNAL_PREFIX}/include
    ${CMAKE_ARCH_LIB_PATH}/include
    ${OPENSSL_INCLUDE_DIR}
)

set(LIBRARY_DEPENDENCIES
    kinetic_client
    ${KERNEL_MEM_DIR}/kernel_mem_mgr.a
    ${CMAKE_ARCH_LIB_PATH}/lib/libglog.a
    ${CMAKE_ARCH_LIB_PATH}/lib/libgflags.a
    ${OPENSSL_LIBRARIES}
    ${CMAKE_ARCH_LIB_PATH}/lib/libprotobuf.a
    ${CMAKE_ARCH_LIB_PATH}/lib/libgtest.a
    ${CMAKE_ARCH_LIB_PATH}/lib/libgmock.a
    ${CMAKE_THREAD_LIBS_INIT}
    dl
)

add_library(kinetic_client
    src/main/generated/kinetic_client.pb.cc
    src/main/hmac_provider.cc
    src/main/kinetic_connection_factory.cc
    src/main/nonblocking_kinetic_connection.cc
    src/main/threadsafe_nonblocking_kinetic_connection.cc
    src/main/nonblocking_packet.cc
    src/main/nonblocking_packet_writer_factory.cc
    src/main/nonblocking_packet_service.cc
    src/main/nonblocking_packet_sender.cc
    src/main/nonblocking_packet_receiver.cc
    src/main/nonblocking_string.cc
    src/main/socket_wrapper.cc
    src/main/blocking_kinetic_connection.cc
    src/main/threadsafe_blocking_kinetic_connection.cc
    src/main/status_code.cc
    src/main/byte_stream.cc
    src/main/incoming_string_value.cc
    src/main/incoming_buff_value.cc
    src/main/message_stream.cc
    src/main/outgoing_string_value.cc
    src/main/reader_writer.cc
    src/main/ssl_reader_writer.cc
    src/main/key_range_iterator.cc
    src/main/LargeMemory.cc
)
add_dependencies(kinetic_client OpenSSL::Crypto OpenSSL::SSL glog gtest gmock)

add_executable(${TEST_BINARY}
    src/test/kinetic_cpp_client_test.cc
    src/test/nonblocking_kinetic_connection_test.cc
    src/test/nonblocking_packet_service_test.cc
    src/test/nonblocking_packet_sender_test.cc
    src/test/nonblocking_packet_receiver_test.cc
    src/test/nonblocking_packet_test.cc
    src/test/nonblocking_string_test.cc
    src/test/hmac_provider_test.cc
    src/test/message_stream_test.cc
    src/test/string_value_test.cc
)
target_link_libraries(${TEST_BINARY} ${LIBRARY_DEPENDENCIES})

add_executable(${INTEGRATION_TEST_BINARY}
    src/integration_test/delete_test.cc
    src/integration_test/get_test.cc
    src/integration_test/nonexistent_server_test.cc
    src/integration_test/put_test.cc
    src/integration_test/blocking_smoketest.cc
    src/test/kinetic_cpp_client_test.cc
)
target_link_libraries(${INTEGRATION_TEST_BINARY} ${LIBRARY_DEPENDENCIES})

add_executable(ise src/test/ise.cc)
target_link_libraries(ise ${LIBRARY_DEPENDENCIES})

# Rule for running unit tests
add_custom_target(check
    COMMAND ${TEST_BINARY_PATH} --gtest_output=xml:gtestresults.xml
    DEPENDS ${TEST_BINARY_PATH}
)

# Rule for running integration tests
add_custom_target(integration_test
    COMMAND ${INTEGRATION_TEST_BINARY_PATH} --gtest_output=xml:integrationresults.xml
    DEPENDS ${INTEGRATION_TEST_BINARY_PATH}
)

# Rules for running unit and integration tests under Valgrind
add_custom_target(test_valgrind
    COMMAND valgrind --leak-check=full --show-reachable=yes --track-fds=yes --suppressions=${kinetic_cpp_client_SOURCE_DIR}/valgrind_linux.supp ${TEST_BINARY_PATH}
    DEPENDS ${TEST_BINARY_PATH}
)
add_custom_target(integration_test_valgrind
    COMMAND valgrind --leak-check=full --show-reachable=yes --track-fds=yes --suppressions=${kinetic_cpp_client_SOURCE_DIR}/valgrind_linux.supp ${INTEGRATION_TEST_BINARY_PATH}
    DEPENDS ${INTEGRATION_TEST_BINARY_PATH}
)

# Rule for generating docs
add_custom_target(doc
    doxygen ${kinetic_cpp_client_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${kinetic_cpp_client_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES docs)

# Rule for linting
add_custom_target(lint
    ./bin/lint.sh
    WORKING_DIRECTORY ${kinetic_cpp_client_SOURCE_DIR}
    COMMENT "Running style checker" VERBATIM
)
