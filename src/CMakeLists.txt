include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# Note that headers are optional, and do not affect add_library, but they will not
# show up in IDEs unless they are listed in add_library.

# Get header files
file(GLOB HEADER_LIST CONFIGURE_DEPENDS "${dc_dump_SOURCE_DIR}/include/*.h")

# Get source files
file(GLOB SOURCE_LIST CONFIGURE_DEPENDS "${dc_dump_SOURCE_DIR}/src/*.c")

# Make an static library
add_executable(dc_dump ${SOURCE_LIST} ${HEADER_LIST})

# We need this directory, and users of our library will need it too
target_include_directories(dc_dump PUBLIC ../include)

# All users of this library will need at least C11
target_compile_features(dc_dump PUBLIC c_std_11)
target_compile_options(dc_dump PRIVATE -fstack-protector-all)
target_compile_options(dc_dump PRIVATE -Wpedantic -Wall -Wextra)
target_compile_options(dc_dump PRIVATE -Wdouble-promotion -Wformat-nonliteral -Wformat-security -Wformat-y2k -Wnull-dereference -Winit-self -Wmissing-include-dirs -Wswitch-default -Wswitch-enum -Wunused-local-typedefs -Wstrict-overflow=5 -Wmissing-noreturn -Walloca -Wframe-address -Wfloat-equal -Wdeclaration-after-statement -Wshadow -Wpointer-arith -Wabsolute-value -Wundef -Wexpansion-to-defined -Wunused-macros -Wno-endif-labels -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wdangling-else -Wdate-time -Wempty-body -Wsign-conversion -Wfloat-conversion -Waggregate-return -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Wpacked -Wpadded -Wredundant-decls -Wnested-externs -Winline -Winvalid-pch -Wlong-long -Wvariadic-macros -Wdisabled-optimization -Wstack-protector -Woverlength-strings )

find_library(LIBDC_FSM dc_fsm REQUIRED)
if(LIBDC_FSM)
    target_link_libraries(dc_dump ${LIBDC_FSM})
endif()

find_library(LIBDC_UTIL dc_util REQUIRED)
if(LIBDC_UTIL)
    target_link_libraries(dc_dump ${LIBDC_UTIL})
endif()

find_library(LIBDC_APPLICATION dc_application REQUIRED)
if(LIBDC_UTIL)
    target_link_libraries(dc_dump ${LIBDC_APPLICATION})
endif()

find_library(LIBCONFIG config REQUIRED)
if(LIBCONFIG)
    target_link_libraries(dc_dump ${LIBCONFIG})
endif()

set_target_properties(dc_dump PROPERTIES OUTPUT_NAME "dcdump")
install(TARGETS dc_dump DESTINATION bin)

# IDEs should put the headers in a nice place
source_group(
        TREE "${PROJECT_SOURCE_DIR}/include"
        PREFIX "Header Files"
        FILES ${HEADER_LIST}
)
