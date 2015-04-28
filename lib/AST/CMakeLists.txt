set(LLVM_LINK_COMPONENTS support)

add_flang_library(flangAST
	ASTContext.cpp
	Decl.cpp
	DeclGroup.cpp
	DeclarationName.cpp
	Expr.cpp
  ExprConstant.cpp
  ExprArray.cpp
	FormatSpec.cpp
  FormatItem.cpp
  IOSpec.cpp
	Stmt.cpp
  ASTDumper.cpp
	Type.cpp
  IntrinsicFunctions.cpp
  )

add_dependencies(flangAST
  FlangDiagnosticCommon
  FlangDeclNodes
  FlangStmtNodes
  FlangExprNodes
  )

target_link_libraries(flangAST
  flangBasic
  )
