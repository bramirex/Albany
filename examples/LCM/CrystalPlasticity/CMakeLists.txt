
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/run_exodiff_test.py
               ${CMAKE_CURRENT_BINARY_DIR}/run_exodiff_test.py COPYONLY)

add_subdirectory(SingleElement2d)
add_subdirectory(SingleElement3d)
add_subdirectory(SingleSlipPlane)
add_subdirectory(SingleSlipPlaneHard)
add_subdirectory(SingleSlipPlaneHardPowerLaw)
add_subdirectory(SingleSlipPlaneOffKilterLoading)
add_subdirectory(RubiksCube)
