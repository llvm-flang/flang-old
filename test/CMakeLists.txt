message("Making Flang test configuration files")

set(FLANG_PATH_TO_LLVM_LIT ${FLANG_PATH_TO_LLVM_BUILD}/bin/llvm-lit)
set(LLVM_SOURCE_DIR ${FLANG_PATH_TO_LLVM_SOURCE})
set(LLVM_BINARY_DIR ${FLANG_PATH_TO_LLVM_BUILD})
set(LLVM_TOOLS_DIR ${FLANG_PATH_TO_LLVM_BUILD}/tools)
set(LLVM_LIBS_DIR ${FLANG_PATH_TO_LLVM_BUILD}/lib)

configure_file(lit.site.cfg.in  ${CMAKE_SOURCE_DIR}/test/lit.site.cfg)
configure_file(TestRunner.sh.in ${CMAKE_SOURCE_DIR}/test/TestRunner.sh)


