include_directories(
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/common
	${PROJECT_SOURCE_DIR}/putils
	${PROJECT_SOURCE_DIR}/putils/mediator
)

aux_source_directory(. SRCS)

# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_executable(example ${SRCS})