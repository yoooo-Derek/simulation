build_lib(
  LIBNAME tl-ocs
  SOURCE_FILES model/config/simulation-config.cc
  HEADER_FILES model/config/simulation-config.h
  LIBRARIES_TO_LINK ${libcore}
  TEST_SOURCES test/tl-ocs-config-test-suite.cc
)

