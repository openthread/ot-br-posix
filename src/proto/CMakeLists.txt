find_package(Protobuf REQUIRED)

# Set up the output path.
SET(MESSAGE_DIR ${PROJECT_BINARY_DIR}/src/proto)
if(EXISTS "${MESSAGE_DIR}" AND IS_DIRECTORY "${MESSAGE_DIR}")
    SET(PROTO_GEN_DIR ${MESSAGE_DIR})
else()
    file(MAKE_DIRECTORY ${MESSAGE_DIR})
    SET(PROTO_GEN_DIR ${MESSAGE_DIR})
endif()

# Set up protoc flags.
LIST(APPEND PROTO_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR})
# Retrieve all proto files.
file(GLOB_RECURSE MSG_PROTOS ${CMAKE_CURRENT_SOURCE_DIR}/*.proto)
set(MESSAGE_SRC "")
set(MESSAGE_HDRS "")

foreach(msg ${MSG_PROTOS})
    get_filename_component(FIL_WE ${msg} NAME_WE)

    list(APPEND MESSAGE_SRC "${PROTO_GEN_DIR}/${FIL_WE}.pb.cc")
    list(APPEND MESSAGE_HDRS "${PROTO_GEN_DIR}/${FIL_WE}.pb.h")

		# Define protoc command.
    add_custom_command(
        OUTPUT "${PROTO_GEN_DIR}/${FIL_WE}.pb.cc"
                "${PROTO_GEN_DIR}/${FIL_WE}.pb.h"
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out  ${PROTO_GEN_DIR}
            -I ${CMAKE_CURRENT_SOURCE_DIR}
            ${msg}
        DEPENDS ${msg}
        COMMENT "Running C++ protocol buffer compiler on ${msg}"
        VERBATIM
    )
endforeach()

# Set the protoc output files as GENERATED.
set_source_files_properties(${MESSAGE_SRC} ${MESSAGE_HDRS}
    PROPERTIES GENERATED TRUE)

# Add custom targets so that proto files will be generated only changed.
add_custom_target(generate_message ALL
    DEPENDS ${MESSAGE_SRC} ${MESSAGE_HDRS}
    COMMENT "generate message target"
    VERBATIM
)
