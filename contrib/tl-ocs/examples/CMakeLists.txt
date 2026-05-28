build_lib_example(
  NAME tl-ocs-minimal
  SOURCE_FILES tl-ocs-minimal.cc
  LIBRARIES_TO_LINK ${libtl-ocs}
)

build_lib_example(
  NAME tl-ocs-eps-topology-example
  SOURCE_FILES tl-ocs-eps-topology-example.cc
  LIBRARIES_TO_LINK ${libtl-ocs}
)
