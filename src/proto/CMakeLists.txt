find_package(Protobuf REQUIRED)

# Set up the output path.
set(PROTO_GEN_DIR ${PROJECT_SOURCE_DIR}/build/src/proto)
if(NOT (EXISTS "${PROTO_GEN_DIR}" AND IS_DIRECTORY "${PROTO_GEN_DIR}"))
    file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
endif()

# Retrieve all proto files.
file(GLOB_RECURSE MSG_PROTOS ${CMAKE_CURRENT_SOURCE_DIR}/*.proto)
set(PROTO_SRC "")
set(PROTO_HDRS "")

foreach(msg ${MSG_PROTOS})
    get_filename_component(FIL_WE ${msg} NAME_WE)

    list(APPEND PROTO_SRC "${PROTO_GEN_DIR}/${FIL_WE}.pb.cc")
    list(APPEND PROTO_HDRS "${PROTO_GEN_DIR}/${FIL_WE}.pb.h")

    # Define protoc command.
    add_custom_command(
        OUTPUT "${PROTO_GEN_DIR}/${FIL_WE}.pb.cc"
                "${PROTO_GEN_DIR}/${FIL_WE}.pb.h"
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --proto_path ${CMAKE_CURRENT_SOURCE_DIR}
            --cpp_out  ${PROTO_GEN_DIR}
            ${msg}
        DEPENDS ${msg}
        COMMENT "Running C++ protocol buffer compiler on ${msg}"
        VERBATIM
    )
endforeach()

# Set the protoc output files as GENERATED.
set_source_files_properties(${PROTO_SRC} ${PROTO_HDRS}
    PROPERTIES GENERATED TRUE
    # Add flag to fix the issue https://github.com/protocolbuffers/protobuf/issues/7140
    COMPILE_FLAGS -Wno-array-bounds)

# Add custom targets so that proto files will be generated only when changed.
add_custom_target(generate_message ALL
    DEPENDS ${PROTO_SRC} ${PROTO_HDRS}
    COMMENT "generate message target"
    VERBATIM
)

add_library(otbr-proto STATIC
    ${PROTO_SRC}
    ${PROTO_HDRS}
)

find_package(Protobuf REQUIRED)
target_link_libraries(otbr-proto
    protobuf::libprotobuf-lite
)
