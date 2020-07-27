add_test(
    NAME rest-server
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test-rest-server
)

set_tests_properties(rest-server PROPERTIES
        ENVIRONMENT "PYTHON_EXECUTABLE=${PYTHON_EXECUTABLE};CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR};CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}"
)

set_tests_properties(rest-server PROPERTIES
                    LABELS "TESTRESTPYTHON" 
)