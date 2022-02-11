
#
#  Copyright (c) 2022, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

find_package(Threads REQUIRED)

# add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wno-ignored-qualifiers;-Wno-redundant-move;-Wno-attributes;-Wno-unused-but-set-parameter;-Wno-unused-parameter;-w>")

add_subdirectory(../../third_party/grpc/ ${CMAKE_CURRENT_BINARY_DIR}/grpc EXCLUDE_FROM_ALL)
message(STATUS "Using gRPC via add_subdirectory.")

set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
if(CMAKE_CROSSCOMPILING)
  find_program(_PROTOBUF_PROTOC protoc)
else()
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()
set(_GRPC_GRPCPP grpc++)
if(CMAKE_CROSSCOMPILING)
  find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
endif()

get_filename_component(otbr_proto "protos/otbr.proto" ABSOLUTE)
get_filename_component(otbr_proto_path "${otbr_proto}" PATH)

set(otbr_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/otbr.pb.cc")
set(otbr_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/otbr.pb.h")
set(otbr_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/otbr.grpc.pb.cc")
set(otbr_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/otbr.grpc.pb.h")
add_custom_command(
      OUTPUT "${otbr_proto_srcs}" "${otbr_proto_hdrs}" "${otbr_grpc_srcs}" "${otbr_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${otbr_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${otbr_proto}"
      DEPENDS "${otbr_proto}")

add_library(otbr_grpc_proto
  ${otbr_grpc_srcs}
  ${otbr_grpc_hdrs}
  ${otbr_proto_srcs}
  ${otbr_proto_hdrs})
target_link_libraries(otbr_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(otbr_grpc_proto PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

add_library(otbr-grpc
  grpc_server.hpp
  grpc_server.cpp
  otbr_impl.hpp
  otbr_impl.cpp
)

target_include_directories(otbr-grpc PUBLIC ../../third_party/grpc/repo/third_party/protobuf/src/)

target_link_libraries(otbr-grpc PUBLIC otbr_grpc_proto otbr-common)
