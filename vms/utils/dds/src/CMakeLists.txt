cmake_minimum_required(VERSION 3.15)
project(xluuv_dds CXX)
enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(OpenDDS REQUIRED)
find_package(CURL REQUIRED)

if( PULL_GRPC )
  include(FetchContent)
  FetchContent_Declare(
    gRPC
    GIT_REPOSITORY https://github.com/grpc/grpc
    GIT_TAG        v1.48.0  # e.g v1.28.0
    GIT_SHALLOW    TRUE
  )
  set(FETCHCONTENT_QUIET OFF)
  FetchContent_MakeAvailable(gRPC)

  # gRPC/protobuf code generation
  # find the proto file
  get_filename_component(ccc_proto "../messages.proto" ABSOLUTE)
  get_filename_component(ccc_proto_path "${ccc_proto}" PATH)

  # define path to genenrate sources
  set(ccc_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/messages.pb.cc")
  set(ccc_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/messages.pb.h")
  set(ccc_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/messages.grpc.pb.cc")
  set(ccc_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/messages.grpc.pb.h")

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/")

  # generate the sources from the protobuf definitions
  add_custom_command(
        OUTPUT "${ccc_proto_srcs}" "${ccc_proto_hdrs}" "${ccc_grpc_srcs}" "${ccc_grpc_hdrs}"
        COMMAND "$<TARGET_FILE:protoc>"
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/"
          --cpp_out "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/"
          --proto_path "${CMAKE_CURRENT_BINARY_DIR}/_deps/grpc-src/third_party/protobuf/src/" # include protobuf common types
          -I "${ccc_proto_path}"
          --plugin=protoc-gen-grpc="$<TARGET_FILE:grpc_cpp_plugin>"
          "${ccc_proto}"
        DEPENDS "${ccc_proto}"
  )
endif()

# include the generated header files
include_directories(SYSTEM 
  "${CMAKE_CURRENT_BINARY_DIR}/grpc_generated/"
  "${CMAKE_CURRENT_BINARY_DIR}/../include/"
)


# Boost ASIO for bc-proxy
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS serialization REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIR})

# IDL TypeSupport Library
add_library(autopilot_idl)
add_library(physical_state_idl)
OPENDDS_TARGET_SOURCES(autopilot_idl PUBLIC "Autopilot.idl")
OPENDDS_TARGET_SOURCES(physical_state_idl PUBLIC "PhysicalState.idl")
target_link_libraries(autopilot_idl PUBLIC OpenDDS::Dcps)
target_link_libraries(physical_state_idl PUBLIC OpenDDS::Dcps)

set(opendds_libs
  OpenDDS::Dcps # Core OpenDDS Library
  OpenDDS::InfoRepoDiscovery OpenDDS::Tcp # For run_test.pl
  OpenDDS::Rtps OpenDDS::Rtps_Udp # For run_test.pl --rtps
  autopilot_idl
  physical_state_idl
)

if( PULL_GRPC )
  # add the grpc_generated library as target
  # ccc_grpc_proto
  add_library(ccc_grpc_proto
    ${ccc_grpc_srcs}
    ${ccc_grpc_hdrs}
    ${ccc_proto_srcs}
    ${ccc_proto_hdrs}
  )
  target_link_libraries(ccc_grpc_proto
    # ${_REFLECTION}
    # ${_GRPC_GRPCPP}
    # ${_PROTOBUF_LIBPROTOBUF}
    grpc++
    libprotobuf
  )
endif()


if( PULL_GRPC )
  # CCC proxy
  add_executable(ccc-proxy
    cccproxy/Main.cpp
    cccproxy/SensorsDRLImpl.cpp
    cccproxy/ActuatorsDRLImpl.cpp
    cccproxy/AivdmMessageDRLImpl.cpp
    cccproxy/MissionReportDRLImpl.cpp
    cccproxy/APReportDRLImpl.cpp
    cccproxy/XLUUVServiceImpl.cpp
    cccproxy/ColregStatusDRLImpl.cpp
  )
  target_link_libraries(ccc-proxy
    ${opendds_libs}
    ${Boost_LIBRARIES}
    ccc_grpc_proto
    grpc++
    libprotobuf
  )
endif()

# Physical state proxy for test integration with BC
add_executable(bc-sen-proxy
  bcsenproxy/Main.cpp
  bcsenproxy/SenWorker.cpp
  bcsenproxy/AisWorker.cpp
)
target_link_libraries(bc-sen-proxy
  ${opendds_libs}
  ${Boost_LIBRARIES}
)

add_executable(bc-act-proxy
  bcactproxy/Main.cpp
  bcactproxy/ActuatorsDRLImpl.cpp
)
target_link_libraries(bc-act-proxy
  ${opendds_libs}
  ${Boost_LIBRARIES}
)

  #  ${Boost_SYSTEM_LIBRARY}

# Autopilot executable
add_executable(autopilot
  autopilot/Main.cpp
  autopilot/AutopilotController.cpp
  autopilot/MissionController.cpp
  autopilot/PidController.cpp
  autopilot/RouteDRLImpl.cpp
  autopilot/LoiterPositionDRLImpl.cpp
  autopilot/DiveProcedureDRLImpl.cpp
  autopilot/MissionDRLImpl.cpp
  autopilot/MissionCommandDRLImpl.cpp
  autopilot/AutopilotCommandDRLImpl.cpp
  autopilot/ProcedureActivationDRLImpl.cpp
  autopilot/SensorsDRLImpl.cpp
  autopilot/AivdmMessageDRLImpl.cpp
)
target_link_libraries(autopilot ${opendds_libs})

if( CMAKE_COMPILER_IS_GNUCC )
  if( PULL_GRPC )
    target_compile_options(ccc-proxy PRIVATE -Wall -Wextra -Wno-unused-parameter)
  endif()
  target_compile_options(autopilot PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_compile_options(bc-sen-proxy PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_compile_options(bc-act-proxy PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
