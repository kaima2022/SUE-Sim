build_lib_example(
  NAME sue-simple-example
  SOURCE_FILES sue-simple-example.cc
  LIBRARIES_TO_LINK ${libsue-sim-module}
                    ${libinternet}
                    ${libapplications}
)