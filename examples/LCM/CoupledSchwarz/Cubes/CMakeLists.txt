##*****************************************************************//
##    Albany 2.0:  Copyright 2012 Sandia Corporation               //
##    This Software is released under the BSD license detailed     //
##    in the file "license.txt" in the top-level Albany directory  //
##*****************************************************************//

# Copy Input file from source to binary dir
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cube0.exo
               ${CMAKE_CURRENT_BINARY_DIR}/cube0.exo COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cube1.exo
               ${CMAKE_CURRENT_BINARY_DIR}/cube1.exo COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cube0.xml
               ${CMAKE_CURRENT_BINARY_DIR}/cube0.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cube1.xml
               ${CMAKE_CURRENT_BINARY_DIR}/cube1.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cubes.xml
               ${CMAKE_CURRENT_BINARY_DIR}/cubes.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cube0.xml.orig
               ${CMAKE_CURRENT_BINARY_DIR}/cube0.xml.orig COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cube1.xml.orig
               ${CMAKE_CURRENT_BINARY_DIR}/cube1.xml.orig COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/materials0.xml
               ${CMAKE_CURRENT_BINARY_DIR}/materials0.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/materials1.xml
               ${CMAKE_CURRENT_BINARY_DIR}/materials1.xml COPYONLY)

