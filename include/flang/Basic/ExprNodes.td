class AttrSubject;

class Expr<bit abstract = 0> : AttrSubject {
  bit Abstract = abstract;
}

class DExpr<Expr base, bit abstract = 0> : Expr<abstract> {
  Expr Base = base;
}

// Constants
def ConstantExpr : Expr;
def IntegerConstantExpr : DExpr<ConstantExpr>;
def RealConstantExpr : DExpr<ConstantExpr>;
def ComplexConstantExpr : DExpr<ConstantExpr>;
def CharacterConstantExpr : DExpr<ConstantExpr>;
def BOZConstantExpr : DExpr<ConstantExpr>;
def LogicalConstantExpr : DExpr<ConstantExpr>;
def RepeatedConstantExpr : Expr;

def FunctionRefExpr : Expr;
def VarExpr : Expr;
def UnresolvedIdentifierExpr : Expr;
def UnaryExpr : Expr;
def DefinedUnaryOperatorExpr : DExpr<UnaryExpr>;
def ImplicitCastExpr : Expr;

def BinaryExpr : Expr;
def DefinedBinaryOperatorExpr : DExpr<BinaryExpr>;

def DesignatorExpr : Expr<1>;
def MemberExpr : DExpr<DesignatorExpr>;
def SubstringExpr : DExpr<DesignatorExpr>;
def ArrayElementExpr : DExpr<DesignatorExpr>;
def ArraySectionExpr : DExpr<DesignatorExpr>;
def ImplicitArrayOperationExpr : Expr<1>;
def ImplicitArrayPackExpr : DExpr<ImplicitArrayOperationExpr>;
def ImplicitTempArrayExpr : DExpr<ImplicitArrayOperationExpr>;

// Call Expressions
def CallExpr : Expr;
def IntrinsicCallExpr : Expr;

//Other
def ImpliedDoExpr : Expr;
def ArrayConstructorExpr : Expr; // (/ /)
def TypeConstructorExpr : Expr;
def RangeExpr : Expr; // a : b
def StridedRangeExpr : DExpr<RangeExpr>;
