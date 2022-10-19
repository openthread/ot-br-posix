find_package(Protobuf REQUIRED)

# Set up the output path.
set(PROTO_GEN_DIR ${PROJECT_SOURCE_DIR}/build/src)
set(MESSAGE_DIR ${PROTO_GEN_DIR}/proto)
if(NOT (EXISTS "${MESSAGE_DIR}" AND IS_DIRECTORY "${MESSAGE_DIR}"))
    file(MAKE_DIRECTORY ${MESSAGE_DIR})
endif()

# Retrieve all proto files.
file(GLOB_RECURSE MSG_PROTOS ${CMAKE_CURRENT_SOURCE_DIR}/*.proto)
set(MESSAGE_SRC "")
set(MESSAGE_HDRS "")

foreach(msg ${MSG_PROTOS})
    get_filename_component(FIL_WE ${msg} NAME_WE)

    list(APPEND MESSAGE_SRC "${MESSAGE_DIR}/${FIL_WE}.pb.cc")
    list(APPEND MESSAGE_HDRS "${MESSAGE_DIR}/${FIL_WE}.pb.h")

    # Define protoc command.
    add_custom_command(
        OUTPUT "${MESSAGE_DIR}/${FIL_WE}.pb.cc"
                "${MESSAGE_DIR}/${FIL_WE}.pb.h"
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --proto_path ${CMAKE_CURRENT_SOURCE_DIR}
            --cpp_out  ${MESSAGE_DIR}
            ${msg}
        DEPENDS ${msg}
        COMMENT "Running C++ protocol buffer compiler on ${msg}"
        VERBATIM
    )
endforeach()

# Set the protoc output files as GENERATED.
set_source_files_properties(${MESSAGE_SRC} ${MESSAGE_HDRS}
    PROPERTIES GENERATED TRUE)

# Add custom targets so that proto files will be generated only when changed.
add_custom_target(generate_message ALL
    DEPENDS ${MESSAGE_SRC} ${MESSAGE_HDRS}
    COMMENT "generate message target"
    VERBATIM
)

add_library(otbr-proto STATIC
    ${MESSAGE_SRC}
    ${MESSAGE_HDRS}
)

find_package(Protobuf REQUIRED)
target_link_libraries(otbr-proto
    protobuf::libprotobuf-lite
)
