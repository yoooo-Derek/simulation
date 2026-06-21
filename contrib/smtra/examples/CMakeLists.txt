build_lib_example(
  NAME smtra-minimal
  SOURCE_FILES smtra-minimal.cc
  LIBRARIES_TO_LINK ${libsmtra}
)

build_lib_example(
  NAME smtra-dragonfly-plus-topology-example
  SOURCE_FILES smtra-dragonfly-plus-topology-example.cc
  LIBRARIES_TO_LINK ${libsmtra}
)
