#Add a parallel test (MPI)
function(add_mpi_ctest TEST_NAME)

  # Add test executable
  set(TEST_SOURCE ${TEST_NAME}.cpp)
  set(TEST_EXE ${TEST_NAME})
  add_executable(${TEST_EXE} ${TEST_SOURCE})

  # Include header files and link libraries
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${TEST_EXE} ${MPI_CXX_LIBRARIES})
  target_link_libraries(${TEST_EXE} lbann)
  if(LBANN_HAS_TBINF)
    target_link_libraries(${TEST_EXE} TBinf)
  endif()

  # ctest with 1, 2, and 4 MPI processes
  set(NUM_PROCS 1)
  while(8 GREATER ${NUM_PROCS})
    add_test("${TEST_NAME}_${NUM_PROCS}"
      ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${NUM_PROCS} ${MPIEXEC_PREFLAGS}
      ${CMAKE_CURRENT_BINARY_DIR}/${TEST_EXE})
    math(EXPR NUM_PROCS "${NUM_PROCS} * 2")
  endwhile()

endfunction()

# Parallel Tests
add_mpi_ctest( quantizer_bm )
add_mpi_ctest( comm_test )
add_mpi_ctest( quantizer_test )
add_mpi_ctest( collective_test )
add_mpi_ctest( lbann_self_test )
