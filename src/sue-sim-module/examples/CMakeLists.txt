build_lib_example(
  NAME sue-sim-module-example
  SOURCE_FILES sue-sim-module-example.cc
  LIBRARIES_TO_LINK ${libsue-sim-module}
                    ${libinternet}
                    ${libapplications}
)

build_lib_example(
  NAME sue-simple-example
  SOURCE_FILES sue-simple-example.cc
  LIBRARIES_TO_LINK ${libsue-sim-module}
                    ${libinternet}
                    ${libapplications}
)