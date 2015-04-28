add_flang_executable(exprEvaluatorTest
  ExprEvaluator.cpp
  )

target_link_libraries(exprEvaluatorTest
  flangAST
  flangFrontend
  flangParse
  flangSema
  flangBasic
  flangCodeGen
  )
