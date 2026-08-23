idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
target_compile_options(${COMPONENT_LIB}
  PRIVATE
    -std=c++20
    -Wall
    -Wextra
    -Wno-missing-field-initializers)
