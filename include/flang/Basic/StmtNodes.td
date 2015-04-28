class AttrSubject;

class Stmt<bit abstract = 0> : AttrSubject {
  bit Abstract = abstract;
}

class DStmt<Stmt base, bit abstract = 0> : Stmt<abstract> {
  Stmt Base = base;
}

// Statements
def ConstructPartStmt : Stmt;
def DeclStmt : Stmt;
def CompoundStmt : Stmt;
def ProgramStmt : Stmt;
def UseStmt : Stmt;
def ImportStmt : Stmt;
def ImplicitStmt : Stmt;
def DimensionStmt : Stmt;
def FormatStmt : Stmt;
def EntryStmt : Stmt;
def AsynchronousStmt : Stmt;
def ParameterStmt : Stmt;
def ExternalStmt : Stmt;
def IntrinsicStmt : Stmt;
def SaveStmt : Stmt;
def EquivalenceStmt : Stmt;
def DataStmt : Stmt;

// executable statements
def BlockStmt  : Stmt;
def AssignStmt : Stmt;
def AssignedGotoStmt : Stmt;
def GotoStmt : Stmt;
def ComputedGotoStmt : Stmt;
def NamedConstructStmt : Stmt<1>;
def IfStmt : DStmt<NamedConstructStmt>;
def CFBlockStmt : DStmt<NamedConstructStmt, 1>;
def DoStmt : DStmt<CFBlockStmt>;
def DoWhileStmt : DStmt<CFBlockStmt>;
def CycleStmt : Stmt;
def ExitStmt : Stmt;
def SelectCaseStmt : DStmt<CFBlockStmt>;
def SelectionCase : DStmt<CFBlockStmt, 1>;
def CaseStmt : DStmt<SelectionCase>;
def DefaultCaseStmt : DStmt<SelectionCase>;
def WhereStmt : Stmt;
def ContinueStmt : Stmt;
def StopStmt : Stmt;
def ReturnStmt : Stmt;
def CallStmt : Stmt;
def AssignmentStmt : Stmt;
def PrintStmt : Stmt;
def WriteStmt : Stmt;
