build_lib(
  LIBNAME tl-ocs
  SOURCE_FILES
    model/config/experiment-config.cc
    model/config/output-config.cc
    model/config/simulation-config.cc
    model/results/csv-schema.cc
    model/results/result-writer.cc
  HEADER_FILES
    model/config/experiment-config.h
    model/config/output-config.h
    model/config/simulation-config.h
    model/results/csv-schema.h
    model/results/result-writer.h
  LIBRARIES_TO_LINK ${libcore}
  TEST_SOURCES
    test/tl-ocs-config-test-suite.cc
    test/tl-ocs-result-writer-test-suite.cc
)
