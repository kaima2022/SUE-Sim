build_lib_example(
  NAME point-to-point-sue-example
  SOURCE_FILES point-to-point-sue-example.cc
  LIBRARIES_TO_LINK ${libpoint-to-point-sue}
                    ${libinternet}
                    ${libapplications}
)