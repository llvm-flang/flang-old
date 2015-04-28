//===--- Stmt.h - Fortran Statements ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the statement objects.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_STMT_H__
#define FLANG_AST_STMT_H__

#include "flang/AST/ASTContext.h"
#include "flang/AST/Expr.h"
#include "flang/Sema/Ownership.h"
#include "flang/Basic/Token.h"
#include "flang/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "flang/Basic/LLVM.h"

namespace flang {

class Expr;
class VarExpr;
class FormatSpec;
class UnitSpec;
class IdentifierInfo;

/// Stmt - The base class for all Fortran statements.
///
class Stmt {
public:
  enum StmtClass {
    NoStmtClass = 0,
#define STMT(CLASS, PARENT) CLASS##Class,
#define STMT_RANGE(BASE, FIRST, LAST) \
        first##BASE##Constant=FIRST##Class, last##BASE##Constant=LAST##Class,
#define LAST_STMT_RANGE(BASE, FIRST, LAST) \
        first##BASE##Constant=FIRST##Class, last##BASE##Constant=LAST##Class
#define ABSTRACT_STMT(STMT)
#include "flang/AST/StmtNodes.inc"
  };

private:
  unsigned StmtID : 16;
  unsigned IsStmtLabelUsed : 1;
  unsigned IsStmtLabelUsedAsGotoTarget : 1;
  unsigned IsStmtLabelUsedAsAssignTarget : 1;
  SourceLocation Loc;
  Expr *StmtLabel;

  Stmt(const Stmt &);           // Do not implement!
  friend class ASTContext;
protected:
  // Make vanilla 'new' and 'delete' illegal for Stmts.
  void* operator new(size_t bytes) throw() {
    assert(0 && "Stmts cannot be allocated with regular 'new'.");
    return 0;
  }
  void operator delete(void* data) throw() {
    assert(0 && "Stmts cannot be released with regular 'delete'.");
  }

  Stmt(StmtClass ID, SourceLocation L, Expr *SLT)
    : StmtID(ID), Loc(L), StmtLabel(SLT),
      IsStmtLabelUsed(0),
      IsStmtLabelUsedAsGotoTarget(0),
      IsStmtLabelUsedAsAssignTarget(0) {}
public:
  virtual ~Stmt();

  /// getStmtClass - Get the Class of the statement.
  StmtClass getStmtClass() const { return StmtClass(StmtID); }

  /// getLocation - Get the location of the statement.
  SourceLocation getLocation() const { return Loc; }
  void setLocation(SourceLocation L) {
    Loc = L;
  }

  virtual SourceLocation getLocStart() const { return Loc; }
  virtual SourceLocation getLocEnd() const { return Loc; }

  inline SourceRange getSourceRange() const {
    return SourceRange(getLocStart(), getLocEnd());
  }

  /// getStmtLabel - Get the statement label for this statement.
  Expr *getStmtLabel() const { return StmtLabel; }

  void setStmtLabel(Expr *E) {
    assert(!StmtLabel);
    StmtLabel = E;
  }

  bool isStmtLabelUsed() const {
    return IsStmtLabelUsed;
  }

  void setStmtLabelUsed() {
    IsStmtLabelUsed = true;
  }

  bool isStmtLabelUsedAsGotoTarget() const {
    return IsStmtLabelUsedAsGotoTarget;
  }

  void setStmtLabelUsedAsGotoTarget() {
    IsStmtLabelUsed = true;
    IsStmtLabelUsedAsGotoTarget = true;
  }

  bool isStmtLabelUsedAsAssignTarget() const {
    return IsStmtLabelUsedAsAssignTarget;
  }

  void setStmtLabelUsedAsAssignTarget() {
    setStmtLabelUsedAsGotoTarget();
    IsStmtLabelUsedAsAssignTarget = true;
  }

  void dump() const;
  void dump(llvm::raw_ostream &OS) const;

  static bool classof(const Stmt*) { return true; }

public:
  // Only allow allocation of Stmts using the allocator in ASTContext or by
  // doing a placement new.
  void *operator new(size_t bytes, ASTContext &C,
                     unsigned alignment = 8) throw() {
    return ::operator new(bytes, C, alignment);
  }

  void *operator new(size_t bytes, ASTContext *C,
                     unsigned alignment = 8) throw() {
    return ::operator new(bytes, *C, alignment);
  }

  void *operator new(size_t bytes, void *mem) throw() {
    return mem;
  }

  void operator delete(void*, ASTContext&, unsigned) throw() { }
  void operator delete(void*, ASTContext*, unsigned) throw() { }
  void operator delete(void*, std::size_t) throw() { }
  void operator delete(void*, void*) throw() { }
};

/// ConstructName - represents the name of the construct statement
struct ConstructName {
  SourceLocation Loc;
  const IdentifierInfo *IDInfo;

  ConstructName()
    : IDInfo(nullptr) {}
  ConstructName(SourceLocation L, const IdentifierInfo *ID)
    : Loc(L), IDInfo(ID) {}
  bool isUsable() const {
    return IDInfo != nullptr;
  }
};

/// ConstructPartStmt - Represents a part of a construct
/// like END, END DO, ELSE
///
class ConstructPartStmt : public Stmt {
public:
  enum ConstructStmtClass {
    EndStmtClass,
    EndProgramStmtClass,
    EndFunctionStmtClass,
    EndSubroutineStmtClass,
    EndDoStmtClass,
    ElseStmtClass,
    EndIfStmtClass,
    EndSelectStmtClass,
    ElseWhereStmtClass,
    EndWhereStmtClass
  };
  ConstructStmtClass ConstructId;
  ConstructName Name;

private:
  ConstructPartStmt(ConstructStmtClass StmtType, SourceLocation Loc,
                    ConstructName name,
                    Expr *StmtLabel);
public:
  static ConstructPartStmt *Create(ASTContext &C, ConstructStmtClass StmtType,
                                   SourceLocation Loc,
                                   ConstructName Name,
                                   Expr *StmtLabel);

  ConstructStmtClass getConstructStmtClass() const {
    return ConstructId;
  }
  ConstructName getName() const {
    return Name;
  }

  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ConstructPartStmtClass;
  }
};

/// DeclStmt - Adaptor class for mixing declarations with statements and
/// expressions.
///
class DeclStmt : public Stmt {
  NamedDecl *Declaration;

  DeclStmt(SourceLocation Loc, NamedDecl *Decl, Expr *StmtLabel);
public:
  static DeclStmt *Create(ASTContext &C, SourceLocation Loc,
                          NamedDecl *Declaration, Expr *StmtLabel);

  NamedDecl *getDeclaration() const {
    return Declaration;
  }

  static bool classof(const DeclStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == DeclStmtClass;
  }
};

/// ListStmt - A statement which has a list of identifiers associated with it.
///
template <typename T = const IdentifierInfo *>
class ListStmt : public Stmt {
  unsigned NumIDs;
  T *IDList;
protected:
  ListStmt(ASTContext &C, Stmt::StmtClass ID, SourceLocation L, ArrayRef<T> IDs,
           Expr *SLT)
    : Stmt(ID, L, SLT) {
    NumIDs = IDs.size();
    IDList = new (C) T [NumIDs];

    for (unsigned I = 0; I != NumIDs; ++I)
      IDList[I] = IDs[I];
  }
  T *getMutableList() {
    return IDList;
  }
public:
  ArrayRef<T> getIDList() const {
    return ArrayRef<T>(IDList, NumIDs);
  }
};

/// CompoundStmt - This represents a group of statements
/// that are bundled together in the source code under one keyword.
///
/// For example, the code PARAMETER (x=1, y=2) will create an AST like:
///   CompoundStmt {
///     ParameterStmt { x = 1 }
///     ParameterStmt { y = 2 }
///   }
class CompoundStmt : public ListStmt<Stmt*> {
  CompoundStmt(ASTContext &C, SourceLocation Loc,
               ArrayRef<Stmt*> Body, Expr *StmtLabel);
public:
  static CompoundStmt *Create(ASTContext &C, SourceLocation Loc,
                              ArrayRef<Stmt*> Body, Expr *StmtLabel);

  ArrayRef<Stmt*> getBody() const {
    return getIDList();
  }
  Stmt *getFirst() const {
    auto Body = getBody();
    if(auto BC = dyn_cast<CompoundStmt>(Body.front()))
      return BC->getFirst();
    return Body.front();
  }

  static bool classof(const CompoundStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == CompoundStmtClass;
  }
};

/// ProgramStmt - The (optional) first statement of the 'main' program.
///
class ProgramStmt : public Stmt {
  const IdentifierInfo *ProgName;
  SourceLocation NameLoc;

  ProgramStmt(const IdentifierInfo *progName, SourceLocation Loc,
              SourceLocation NameL, Expr *SLT)
    : Stmt(ProgramStmtClass, Loc, SLT), ProgName(progName), NameLoc(NameL) {}
  ProgramStmt(const ProgramStmt &); // Do not implement!
public:
  static ProgramStmt *Create(ASTContext &C, const IdentifierInfo *ProgName,
                             SourceLocation L, SourceLocation NameL,
                             Expr *StmtLabel);

  /// getProgramName - Get the name of the program. This may be null.
  const IdentifierInfo *getProgramName() const { return ProgName; }

  /// getNameLocation - Get the location of the program name.
  SourceLocation getNameLocation() const { return NameLoc; }

  static bool classof(const ProgramStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ProgramStmtClass;
  }
};


//===----------------------------------------------------------------------===//
// Specification Part Statements
//===----------------------------------------------------------------------===//

/// UseStmt - A reference to the module it specifies.
///
class UseStmt : public ListStmt<std::pair<const IdentifierInfo *,
                                          const IdentifierInfo *> > {
public:
  enum ModuleNature {
    None,
    IntrinsicStmtClass,
    NonIntrinsic
  };
  typedef std::pair<const IdentifierInfo *, const IdentifierInfo *> RenamePair;
private:
  ModuleNature ModNature;
  const IdentifierInfo *ModName;
  bool Only;

  UseStmt(ASTContext &C, ModuleNature MN, const IdentifierInfo *modName,
          ArrayRef<RenamePair> RenameList, Expr *StmtLabel);

  void init(ASTContext &C, ArrayRef<RenamePair> RenameList);
public:
  static UseStmt *Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName,
                         Expr *StmtLabel);
  static UseStmt *Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName, bool Only,
                         ArrayRef<RenamePair> RenameList,
                         Expr *StmtLabel);

  /// Accessors:
  ModuleNature getModuleNature() const { return ModNature; }
  StringRef getModuleName() const;

  static bool classof(const UseStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == UseStmtClass;
  }
};

/// ImportStmt - Specifies that the named entities from the host scoping unit
/// are accessible in the interface body by host association.
///
class ImportStmt : public ListStmt<> {
  ImportStmt(ASTContext &C, SourceLocation Loc, ArrayRef<const IdentifierInfo*> names,
             Expr *StmtLabel);
public:
  static ImportStmt *Create(ASTContext &C, SourceLocation Loc,
                            ArrayRef<const IdentifierInfo*> Names,
                            Expr *StmtLabel);

  static bool classof(const ImportStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ImportStmtClass;
  }
};

/// ParameterStmt - represents a PARAMETER declaration.
class ParameterStmt : public Stmt {
  const IdentifierInfo *IDInfo;
  Expr *Value;
  ParameterStmt(SourceLocation Loc,const IdentifierInfo *Name,
                Expr *Val, Expr *StmtLabel);
public:
  static ParameterStmt *Create(ASTContext &C, SourceLocation Loc,
                               const IdentifierInfo *Name,
                               Expr *Value, Expr *StmtLabel);

  const IdentifierInfo *getIdentifier() const {
    return IDInfo;
  }
  Expr *getValue() const {
    return Value;
  }

  static bool classof(const ParameterStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ParameterStmtClass;
  }
};

/// ImplicitStmt - Specifies a type, and possibly type parameters, for all
/// implicitly typed data entries whose names begin with one of the letters
/// specified in the statement.
///
class ImplicitStmt : public Stmt {
public:
  typedef std::pair<const IdentifierInfo *, const IdentifierInfo *> LetterSpecTy;
private:
  QualType Ty;
  LetterSpecTy LetterSpec;
  bool None;

  ImplicitStmt(SourceLocation Loc, Expr *StmtLabel);
  ImplicitStmt(SourceLocation Loc, QualType T,
               LetterSpecTy Spec, Expr *StmtLabel);
public:
  static ImplicitStmt *Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);
  static ImplicitStmt *Create(ASTContext &C, SourceLocation Loc, QualType T,
                              LetterSpecTy LetterSpec,
                              Expr *StmtLabel);

  bool isNone() const { return None; }

  QualType getType() const { return Ty; }
  LetterSpecTy getLetterSpec() const { return LetterSpec; }

  static bool classof(const ImplicitStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ImplicitStmtClass;
  }
};

/// DimensionStmt - Specifies the DIMENSION attribute for a named constant.
///
class DimensionStmt : public ListStmt<ArraySpec*> {
  const IdentifierInfo *VarName;

  DimensionStmt(ASTContext &C, SourceLocation Loc, const IdentifierInfo* IDInfo,
                ArrayRef<ArraySpec*> Dims, Expr *StmtLabel);
public:
  static DimensionStmt *Create(ASTContext &C, SourceLocation Loc,
                               const IdentifierInfo* IDInfo,
                               ArrayRef<ArraySpec*> Dims,
                               Expr *StmtLabel);

  const IdentifierInfo *getVariableName() const {
    return VarName;
  }

  static bool classof(const DimensionStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == DimensionStmtClass;
  }
};

class FormatItemList;

/// FormatStmt -
///
class FormatStmt : public Stmt {
  FormatItemList *Items;
  FormatItemList *UnlimitedItems;

  FormatStmt(SourceLocation Loc, FormatItemList *ItemList,
             FormatItemList *UnlimitedItemList, Expr *StmtLabel);
public:
  static FormatStmt *Create(ASTContext &C, SourceLocation Loc,
                            FormatItemList *ItemList,
                            FormatItemList *UnlimitedItemList,
                            Expr *StmtLabel);

  FormatItemList *getItemList() const {
    return Items;
  }
  FormatItemList *getUnlimitedItemList() const {
    return UnlimitedItems;
  }

  static bool classof(const FormatStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == FormatStmtClass;
  }
};

/// EntryStmt -
///
class EntryStmt : public Stmt {
  EntryStmt(SourceLocation Loc, Expr *StmtLabel);
public:
  static EntryStmt *Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  static bool classof(const EntryStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == EntryStmtClass;
  }
};

/// AsynchronousStmt - Specifies the asynchronous attribute for a list of
/// objects.
///
class AsynchronousStmt : public ListStmt<> {
  AsynchronousStmt(ASTContext &C, SourceLocation Loc,
                   ArrayRef<const IdentifierInfo*> objNames,
                   Expr *StmtLabel);
public:
  static AsynchronousStmt *Create(ASTContext &C, SourceLocation Loc,
                                  ArrayRef<const IdentifierInfo*> objNames,
                                  Expr *StmtLabel);

  static bool classof(const AsynchronousStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == AsynchronousStmtClass;
  }
};

/// ExternalStmt - Specifies the external attribute for a list of objects.
///
class ExternalStmt : public Stmt {
  const IdentifierInfo *IDInfo;

  ExternalStmt(SourceLocation Loc, const IdentifierInfo *Name,
               Expr *StmtLabel);
public:
  static ExternalStmt *Create(ASTContext &C, SourceLocation Loc,
                              const IdentifierInfo *Name,
                              Expr *StmtLabel);

  const IdentifierInfo *getIdentifier() const {
    return IDInfo;
  }

  static bool classof(const ExternalStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ExternalStmtClass;
  }
};


/// IntrinsicStmt - Lists the intrinsic functions declared in this program unit.
///
class IntrinsicStmt : public Stmt {
  const IdentifierInfo *IDInfo;

  IntrinsicStmt(SourceLocation Loc, const IdentifierInfo *Name,
                Expr *StmtLabel);
public:
  static IntrinsicStmt *Create(ASTContext &C, SourceLocation Loc,
                               const IdentifierInfo *Name,
                               Expr *StmtLabel);

  const IdentifierInfo *getIdentifier() const {
    return IDInfo;
  }

  static bool classof(const IntrinsicStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == IntrinsicStmtClass;
  }
};

/// SaveStmt - this is a part of SAVE statement.
/// if the identifier is null, this statement represents
/// a SAVE statement without object list.
class SaveStmt : public Stmt {
  const IdentifierInfo *IDInfo;

  SaveStmt(SourceLocation Loc, const IdentifierInfo *Name,
           Expr *StmtLabel);
public:
  static SaveStmt *Create(ASTContext &C, SourceLocation Loc,
                          const IdentifierInfo *Name,
                          Expr *StmtLabel);

  const IdentifierInfo *getIdentifier() const {
    return IDInfo;
  }

  static bool classof(const SaveStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == SaveStmtClass;
  }
};

/// EquivalenceStmt - this is a part of EQUIVALENCE statement.
class EquivalenceStmt : public Stmt, public MultiArgumentExpr {
  EquivalenceStmt(ASTContext &C, SourceLocation Loc,
                  ArrayRef<Expr*> Objects, Expr *StmtLabel);
public:
  static EquivalenceStmt *Create(ASTContext &C, SourceLocation Loc,
                                 ArrayRef<Expr*> Objects,
                                 Expr *StmtLabel);

  ArrayRef<Expr*> getObjects() const {
    return getArguments();
  }

  static bool classof(const EquivalenceStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == EquivalenceStmtClass;
  }
};

/// DataStmt - this is a part of the DATA statement
class DataStmt : public Stmt {
  unsigned NumNames;
  unsigned NumValues;
  Expr **NameList, **ValueList;
  Stmt *Body;

  DataStmt(ASTContext &C, SourceLocation Loc,
           ArrayRef<Expr*> Objects,
           ArrayRef<Expr*> Values, Expr *StmtLabel);
public:
  static DataStmt *Create(ASTContext &C, SourceLocation Loc,
                          ArrayRef<Expr*> Objects,
                          ArrayRef<Expr*> Values, Expr *StmtLabel);

  ArrayRef<Expr*> getObjects() const {
    return ArrayRef<Expr*>(NameList, NumNames);
  }
  ArrayRef<Expr*> getValues() const {
    return ArrayRef<Expr*>(ValueList, NumValues);
  }

  static bool classof(const DataStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == DataStmtClass;
  }
};

//===----------------------------------------------------------------------===//
// Executable Statements
//===----------------------------------------------------------------------===//

/// BlockStmt
class BlockStmt : public ListStmt<Stmt*> {
  BlockStmt(ASTContext &C, SourceLocation Loc,
            ArrayRef<Stmt*> Body);
public:
  static BlockStmt *Create(ASTContext &C, SourceLocation Loc,
                           ArrayRef<Stmt*> Body);

  ArrayRef<Stmt*> getStatements() const {
    return getIDList();
  }

  static bool classof(const BlockStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == BlockStmtClass;
  }
};

/// StmtLabelInteger - an integer big enough to hold the value
/// of a statement label.
typedef uint32_t StmtLabelInteger;

/// StmtLabelReference - a reference to a statement label
struct StmtLabelReference {
  Stmt *Statement;

  StmtLabelReference()
    : Statement(nullptr) {
  }
  inline StmtLabelReference(Stmt *S)
    : Statement(S) {
    assert(S);
  }
};

/// AssignStmt - assigns a statement label to an integer variable.
class AssignStmt : public Stmt {
  StmtLabelReference Address;
  Expr *Destination;
  AssignStmt(SourceLocation Loc, StmtLabelReference Addr, Expr *Dest,
             Expr *StmtLabel);
public:
  static AssignStmt *Create(ASTContext &C, SourceLocation Loc,
                            StmtLabelReference Address,
                            Expr *Destination,
                            Expr *StmtLabel);

  inline StmtLabelReference getAddress() const {
    return Address;
  }
  void setAddress(StmtLabelReference Address);
  inline Expr *getDestination() const {
    return Destination;
  }

  static bool classof(const AssignStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == AssignStmtClass;
  }
};

/// AssignedGotoStmt - jump to a position determined by an integer
/// variable.
class AssignedGotoStmt : public ListStmt<StmtLabelReference> {
  Expr *Destination;
  AssignedGotoStmt(ASTContext &C, SourceLocation Loc, Expr *Dest,
                   ArrayRef<StmtLabelReference> Vals,
                   Expr *StmtLabel);
public:
  static AssignedGotoStmt *Create(ASTContext &C, SourceLocation Loc,
                                  Expr *Destination,
                                  ArrayRef<StmtLabelReference> AllowedValues,
                                  Expr *StmtLabel);

  inline Expr *getDestination() const {
    return Destination;
  }
  inline ArrayRef<StmtLabelReference> getAllowedValues() const {
    return getIDList();
  }
  void setAllowedValue(size_t I, StmtLabelReference Address);

  static bool classof(const AssignedGotoStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == AssignedGotoStmtClass;
  }
};

/// GotoStmt - an unconditional jump
class GotoStmt : public Stmt {
  StmtLabelReference Destination;
  GotoStmt(SourceLocation Loc, StmtLabelReference Dest, Expr *StmtLabel);
public:
  static GotoStmt *Create(ASTContext &C, SourceLocation Loc,
                          StmtLabelReference Destination,
                          Expr *StmtLabel);

  inline StmtLabelReference getDestination() const {
    return Destination;
  }
  void setDestination(StmtLabelReference Destination);

  static bool classof(const GotoStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == GotoStmtClass;
  }
};

/// ComputedGotoStmt - a computed goto jump
class ComputedGotoStmt : public ListStmt<StmtLabelReference> {
  Expr *E;
  ComputedGotoStmt(ASTContext &C, SourceLocation Loc, Expr *e,
                   ArrayRef<StmtLabelReference> Targets, Expr *StmtLabel);
public:
  static ComputedGotoStmt *Create(ASTContext &C, SourceLocation Loc,
                                  Expr *Expression,
                                  ArrayRef<StmtLabelReference> Targets,
                                  Expr *StmtLabel);

  inline Expr *getExpression() const {
    return E;
  }
  inline ArrayRef<StmtLabelReference> getTargets() const {
    return getIDList();
  }
  void setTarget(size_t I, StmtLabelReference Address);

  static bool classof(const ComputedGotoStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ComputedGotoStmtClass;
  }
};

/// NamedConstructStmt - A base class
/// for named constructs such as if, do, etc.
class NamedConstructStmt : public Stmt {
  ConstructName Name;

protected:
  NamedConstructStmt(StmtClass Type, SourceLocation Loc, Expr *StmtLabel, ConstructName name)
    : Stmt(Type, Loc, StmtLabel), Name(name) {}
public:

  ConstructName getName() const { return Name; }

  static bool classof(const NamedConstructStmt*) { return true; }
  static bool classof(const Stmt *S){
    return S->getStmtClass() >= firstNamedConstructStmtConstant &&
           S->getStmtClass() <= lastNamedConstructStmtConstant;
  }
};

/// IfStmt
/// An if statement is also a control flow statement
class IfStmt : public NamedConstructStmt {
  Expr *Condition;
  Stmt *ThenArm, *ElseArm;

  IfStmt(SourceLocation Loc, Expr *Cond, Expr *StmtLabel, ConstructName Name);
public:
  static IfStmt *Create(ASTContext &C, SourceLocation Loc,
                        Expr *Condition, Expr *StmtLabel, ConstructName Name);

  inline Expr *getCondition() const { return Condition; }
  inline Stmt *getThenStmt() const { return ThenArm; }
  inline Stmt *getElseStmt() const { return ElseArm; }
  void setThenStmt(Stmt *Body);
  void setElseStmt(Stmt *Body);

  static bool classof(const IfStmt*) { return true; }
  static bool classof(const Stmt *S){
    return S->getStmtClass() == IfStmtClass;
  }
};

/// A base class for statements with own body which
/// the program will execute when entering this body.
class CFBlockStmt : public NamedConstructStmt {
  Stmt *Body;
protected:
  CFBlockStmt(StmtClass Type, SourceLocation Loc, Expr *StmtLabel, ConstructName Name);
public:
  Stmt *getBody() const { return Body; }
  void setBody(Stmt *Body);

  static bool classof(const Stmt *S) {
    return S->getStmtClass() >= firstCFBlockStmtConstant &&
           S->getStmtClass() <= lastCFBlockStmtConstant;
  }
};

/// DoStmt
class DoStmt : public CFBlockStmt {
  StmtLabelReference TerminatingStmt;
  VarExpr *DoVar;
  Expr *Init, *Terminate, *Increment;

  DoStmt(SourceLocation Loc, StmtLabelReference TermStmt, VarExpr *DoVariable,
         Expr *InitialParam, Expr *TerminalParam,
         Expr *IncrementationParam, Expr *StmtLabel, ConstructName Name);
public:
  static DoStmt *Create(ASTContext &C,SourceLocation Loc, StmtLabelReference TermStmt,
                        VarExpr *DoVariable, Expr *InitialParam,
                        Expr *TerminalParam,Expr *IncrementationParam,
                        Expr *StmtLabel, ConstructName Name);

  StmtLabelReference getTerminatingStmt() const { return TerminatingStmt; }
  void setTerminatingStmt(StmtLabelReference Stmt);
  VarExpr *getDoVar() const { return DoVar; }
  Expr *getInitialParameter() const { return Init; }
  Expr *getTerminalParameter() const { return Terminate; }
  Expr *getIncrementationParameter() const { return Increment; }

  static bool classof(const DoStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == DoStmtClass;
  }
};

/// DoWhileStmt
class DoWhileStmt : public CFBlockStmt {
  Expr *Condition;

  DoWhileStmt(SourceLocation Loc, Expr *Cond, Expr *StmtLabel, ConstructName Name);
public:
  static DoWhileStmt *Create(ASTContext &C, SourceLocation Loc,
                             Expr *Condition, Expr *StmtLabel, ConstructName Name);

  Expr *getCondition() const { return Condition; }

  static bool classof(const DoWhileStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == DoWhileStmtClass;
  }
};

/// CycleStmt
class CycleStmt : public Stmt {
  ConstructName LoopName;
  Stmt *Loop;
  CycleStmt(SourceLocation Loc, Expr *StmtLabel, Stmt *loop, ConstructName loopName);
public:
  static CycleStmt *Create(ASTContext &C, SourceLocation Loc, Stmt *Loop,
                           Expr *StmtLabel, ConstructName LoopName);

  ConstructName getLoopName() const {
    return LoopName;
  }
  Stmt *getLoop() const {
    return Loop;
  }

  static bool classof(const CycleStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == CycleStmtClass;
  }
};

/// ExitStmt
class ExitStmt : public Stmt {
  ConstructName LoopName;
  Stmt *Loop;
  ExitStmt(SourceLocation Loc, Expr *StmtLabel, Stmt *loop, ConstructName loopName);
public:
  static ExitStmt *Create(ASTContext &C, SourceLocation Loc, Stmt *Loop,
                          Expr *StmtLabel, ConstructName LoopName);

  ConstructName getLoopName() const {
    return LoopName;
  }
  Stmt *getLoop() const {
    return Loop;
  }

  static bool classof(const ExitStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ExitStmtClass;
  }
};

class CaseStmt;
class DefaultCaseStmt;

/// SelectCaseStmt
class SelectCaseStmt : public CFBlockStmt {
  CaseStmt *FirstCase;
  DefaultCaseStmt *DefaultCase;
  Expr *E;

  SelectCaseStmt(SourceLocation Loc, Expr *Operand,
                 Expr *StmtLabel, ConstructName Name);
public:
  static SelectCaseStmt *Create(ASTContext &C, SourceLocation Loc,
                                Expr *Operand, Expr *StmtLabel,
                                ConstructName Name);

  Expr *getOperand() const {
    return E;
  }

  CaseStmt *getFirstCase() const {
    return FirstCase;
  }
  void addCase(CaseStmt *S);

  DefaultCaseStmt *getDefaultCase() const {
    return DefaultCase;
  }
  bool hasDefaultCase() const {
    return DefaultCase != nullptr;
  }
  void setDefaultCase(DefaultCaseStmt *S);

  static bool classof(const SelectCaseStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == SelectCaseStmtClass;
  }
};

class SelectionCase : public CFBlockStmt {
protected:
  SelectionCase(StmtClass StmtKind, SourceLocation Loc,
                Expr *StmtLabel, ConstructName name)
    : CFBlockStmt(StmtKind, Loc, StmtLabel, name) {}
public:

  static bool classof(const Stmt *S) {
    return S->getStmtClass() >= firstSelectionCaseConstant &&
           S->getStmtClass() <= lastSelectionCaseConstant;
  }
};

/// CaseStmt
class CaseStmt : public SelectionCase, public MultiArgumentExpr {
  CaseStmt *Next;

  CaseStmt(ASTContext &C, SourceLocation Loc,
           ArrayRef<Expr*> Values,
           Expr *StmtLabel, ConstructName Name);
public:
  static CaseStmt *Create(ASTContext &C, SourceLocation Loc,
                          ArrayRef<Expr*> Values,
                          Expr *StmtLabel, ConstructName Name);

  CaseStmt *getNextCase() const {
    return Next;
  }
  bool isLastCase() const {
    return Next == nullptr;
  }
  void setNextCase(CaseStmt *S);
  ArrayRef<Expr*> getValues() const {
    return getArguments();
  }

  static bool classof(const CaseStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == CaseStmtClass;
  }
};

/// DefaultCaseStmt
class DefaultCaseStmt : public SelectionCase {
  DefaultCaseStmt(SourceLocation Loc, Expr *StmtLabel,
                  ConstructName Name);
public:
  static DefaultCaseStmt *Create(ASTContext &C, SourceLocation Loc,
                                 Expr *StmtLabel, ConstructName Name);

  static bool classof(const DefaultCaseStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == DefaultCaseStmtClass;
  }
};

/// WhereStmt
class WhereStmt : public Stmt {
  Expr *Mask;
  Stmt *ThenArm, *ElseArm;

  WhereStmt(SourceLocation Loc, Expr *mask, Expr *StmtLabel);
public:
  static WhereStmt *Create(ASTContext &C, SourceLocation Loc,
                           Expr *Mask, Expr *StmtLabel);

  Stmt *getThenStmt() const { return ThenArm; }
  Stmt *getElseStmt() const { return ElseArm; }
  bool hasElseStmt() const { return ElseArm != nullptr; }
  void setThenStmt(Stmt *Body);
  void setElseStmt(Stmt *Body);

  Expr *getMask() const {
    return Mask;
  }

  static bool classof(const WhereStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == WhereStmtClass;
  }
};

/// ContinueStmt
class ContinueStmt : public Stmt {
  ContinueStmt(SourceLocation Loc, Expr *StmtLabel);
public:
  static ContinueStmt *Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  static bool classof(const ContinueStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ContinueStmtClass;
  }
};

/// StopStmt
class StopStmt : public Stmt {
  Expr *StopCode;

  StopStmt(SourceLocation Loc, Expr *stopCode, Expr *StmtLabel);
public:
  static StopStmt *Create(ASTContext &C, SourceLocation Loc, Expr *stopCode, Expr *StmtLabel);

  Expr *getStopCode() const { return StopCode; }

  static bool classof(const StopStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == StopStmtClass;
  }
};

/// ReturnStmt
class ReturnStmt : public Stmt {
  Expr *E;
  ReturnStmt(SourceLocation Loc, Expr *e, Expr *StmtLabel);
public:
  static ReturnStmt *Create(ASTContext &C, SourceLocation Loc, Expr * E,
                            Expr *StmtLabel);

  Expr *getE() const { return E; }

  static bool classof(const ReturnStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == ReturnStmtClass;
  }
};

/// CallStmt
class CallStmt : public Stmt, public MultiArgumentExpr {
  FunctionDecl *Function;
  CallStmt(ASTContext &C, SourceLocation Loc,
           FunctionDecl *Func, ArrayRef<Expr*> Args, Expr *StmtLabel);
public:
  static CallStmt *Create(ASTContext &C, SourceLocation Loc,
                          FunctionDecl *Func, ArrayRef<Expr*> Args,
                          Expr *StmtLabel);

  FunctionDecl *getFunction() const { return Function; }

  static bool classof(const Stmt *S) {
    return S->getStmtClass() == CallStmtClass;
  }
  static bool classof(const CallStmt *) { return true; }
};

/// AssignmentStmt
class AssignmentStmt : public Stmt {
  Expr *LHS;
  Expr *RHS;

  AssignmentStmt(SourceLocation Loc, Expr *lhs, Expr *rhs, Expr *StmtLabel);
public:
  static AssignmentStmt *Create(ASTContext &C, SourceLocation Loc, Expr *LHS,
                                Expr *RHS, Expr *StmtLabel);

  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }

  static bool classof(const AssignmentStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == AssignmentStmtClass;
  }
};

/// PrintStmt
class PrintStmt : public Stmt, protected MultiArgumentExpr {
  FormatSpec *FS;
  PrintStmt(ASTContext &C, SourceLocation L, FormatSpec *fs,
            ArrayRef<Expr*> OutList, Expr *StmtLabel);
public:
  static PrintStmt *Create(ASTContext &C, SourceLocation L, FormatSpec *fs,
                           ArrayRef<Expr*> OutList, Expr *StmtLabel);

  FormatSpec *getFormatSpec() const { return FS; }

  ArrayRef<Expr*> getOutputList() const {
    return getArguments();
  }

  static bool classof(const PrintStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == PrintStmtClass;
  }
};

/// WriteStmt
class WriteStmt : public Stmt, protected MultiArgumentExpr {
  UnitSpec *US;
  FormatSpec *FS;
  WriteStmt(ASTContext &C, SourceLocation Loc, UnitSpec *us,
            FormatSpec *fs, ArrayRef<Expr*> OutList, Expr *StmtLabel);
public:
  static WriteStmt *Create(ASTContext &C, SourceLocation Loc, UnitSpec *US,
                           FormatSpec *FS, ArrayRef<Expr*> OutList,
                           Expr *StmtLabel);

  UnitSpec *getUnitSpec() const { return US; }
  FormatSpec *getFormatSpec() const { return FS; }

  ArrayRef<Expr*> getOutputList() const {
    return getArguments();
  }

  static bool classof(const WriteStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStmtClass() == WriteStmtClass;
  }
};

} // end flang namespace

#endif
