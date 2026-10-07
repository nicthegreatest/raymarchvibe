# DeployShaders.cmake -- keep the shaders in the build directory in step with the
# source tree, and copy them only when the two actually differ.
#
# Run on every build by the RaymarchVibeShaders custom target (see the shader
# deployment block in CMakeLists.txt). It prints nothing when the build directory
# is already current, so an up-to-date build stays quiet.
#
# Why the comparison happens here instead of through CMake file lists:
#   * listing the shader files as DEPENDS of a custom command does not work in
#     this repository. Three preset file names contain "=" (for example
#     "baked - 4-20 =).milk"), which CMake's Makefile generator cannot write into
#     a prerequisite list: the generated rule gets cut short, make warns
#     "overriding recipe for target 'CMakeFiles/shaders-copied.stamp'", and every
#     shader that sorts after those names silently stops being a dependency --
#     so edits to shaders/raymarch_v2.frag and the rest of the tail were never
#     picked up, which is the same silent-stale-shader failure being fixed here.
#   * file(GLOB_RECURSE) is no better on this tree: it returns 342 elements for
#     466 files, the last 124 names folded into one element that contains a stray
#     list separator, so a hand-rolled comparison over that list is wrong too.
# `diff -r -q` compares the two directory trees without going through a CMake
# list at all. If no diff tool is available, the copy just runs every time:
# correctness never depends on it, only the "do nothing when unchanged" part.
#
# Inputs (pass with -D):
#   RAYVIBE_SHADER_SOURCE       shaders/ in the source tree
#   RAYVIBE_SHADER_DESTINATION  the directory the application loads shaders from

foreach(_required RAYVIBE_SHADER_SOURCE RAYVIBE_SHADER_DESTINATION)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "DeployShaders.cmake: ${_required} is not set")
  endif()
endforeach()

set(_reason "")
if(NOT EXISTS "${RAYVIBE_SHADER_DESTINATION}")
  set(_reason "the build directory has no copy of the shaders yet")
else()
  find_program(_deploy_shaders_diff diff)
  if(_deploy_shaders_diff)
    execute_process(
      COMMAND "${_deploy_shaders_diff}" -r -q
              "${RAYVIBE_SHADER_SOURCE}" "${RAYVIBE_SHADER_DESTINATION}"
      RESULT_VARIABLE _diff_result
      OUTPUT_VARIABLE _diff_output
      ERROR_VARIABLE _diff_output)
    if(NOT _diff_result EQUAL 0)
      string(REGEX REPLACE "\n.*" "" _first_difference "${_diff_output}")
      set(_reason "${_first_difference}")
    endif()
  else()
    set(_reason "no diff tool found, so the copy cannot be skipped")
  endif()
endif()

if(NOT _reason)
  return()
endif()

message(STATUS "Copying shaders to build directory (${_reason})")
file(REMOVE_RECURSE "${RAYVIBE_SHADER_DESTINATION}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${RAYVIBE_SHADER_SOURCE}" "${RAYVIBE_SHADER_DESTINATION}"
  RESULT_VARIABLE _copy_result)
if(NOT _copy_result EQUAL 0)
  message(FATAL_ERROR "DeployShaders.cmake: copying ${RAYVIBE_SHADER_SOURCE} to "
                      "${RAYVIBE_SHADER_DESTINATION} failed (${_copy_result})")
endif()
