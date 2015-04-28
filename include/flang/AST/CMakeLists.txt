flang_tablegen(DeclNodes.inc -gen-flang-decl-nodes
  SOURCE ../Basic/DeclNodes.td
  TARGET FlangDeclNodes)

flang_tablegen(StmtNodes.inc -gen-flang-stmt-nodes
  SOURCE ../Basic/StmtNodes.td
  TARGET FlangStmtNodes)

flang_tablegen(ExprNodes.inc -gen-flang-expr-nodes
  SOURCE ../Basic/ExprNodes.td
  TARGET FlangExprNodes)
