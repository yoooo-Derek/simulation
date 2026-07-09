build_lib_example(
  NAME satr-minimal
  SOURCE_FILES satr-minimal.cc
  LIBRARIES_TO_LINK satr
)

build_lib_example(
  NAME satr-dragonfly-plus-topology-example
  SOURCE_FILES satr-dragonfly-plus-topology-example.cc
  LIBRARIES_TO_LINK satr
)
