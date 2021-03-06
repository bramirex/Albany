# 1. Copy Input file from source to binary dir
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis20km_ascii.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis20km_ascii.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis20km.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis20km.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis20km_refine.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis20km_refine.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis10km_ascii.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis10km_ascii.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis10km.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis10km.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis5km_ascii.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis5km_ascii.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis5km.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis5km.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis2km_ascii.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis2km_ascii.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis2km.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis2km.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis_unstruct.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis_unstruct.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis_unstruct_restart.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis_unstruct_restart.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis_adjoint_sensitivity.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis_adjoint_sensitivity.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis_wedge_adjoint_sensitivity.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis_wedge_adjoint_sensitivity.xml COPYONLY)

get_filename_component(testName ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#3. Tests 
#  First Test
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis20km_test.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis20km_test.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_fo_gis20km_testT.xml
               ${CMAKE_CURRENT_BINARY_DIR}/input_fo_gis20km_testT.xml COPYONLY)
if (ALBANY_EPETRA) 
add_test(${testName}_Gis20km ${Albany.exe} input_fo_gis20km_test.xml)
add_test(${testName}_GisUnstructured ${Albany.exe} input_fo_gis_unstruct.xml)
add_test(${testName}_GisRestartUnstructured ${Albany.exe} input_fo_gis_unstruct_restart.xml)
add_test(${testName}_GisAdjointSensitivity ${Albany.exe} input_fo_gis_adjoint_sensitivity.xml)
add_test(${testName}_GisWedgeAdjointSensitivity ${Albany.exe} input_fo_gis_wedge_adjoint_sensitivity.xml)
set_tests_properties(${testName}_GisRestartUnstructured  PROPERTIES DEPENDS ${testName}_GisUnstructured) 
set_tests_properties(${testName}_Gis20km  PROPERTIES LABELS CUDA_TEST) 
endif() 
if(ALBANY_IFPACK2)
add_test(${testName}_Gis20km_Tpetra ${AlbanyT.exe} input_fo_gis20km_testT.xml)
endif(ALBANY_IFPACK2)

#IK, 10/29/13: Convert to tpetra!
IF(ALBANY_STK_PERCEPT AND ALBANY_EPETRA)
add_test(${testName}_Gis20km_refine ${Albany8.exe} input_fo_gis20km_refine.xml)
ENDIF()

#salloc -n 4 -n 4 -p pbatch
#mpirun -np 4 ../../../build/src/Albany input_fo_gis20km.xml
