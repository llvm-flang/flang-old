//===--- Scope.h - Scope interface ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Scope interface.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_SEMA_SCOPE_H__
#define FLANG_SEMA_SCOPE_H__

#include "flang/Basic/Diagnostic.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/FormatSpec.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include <map>

namespace flang {

class Decl;
class DeclContext;
class UsingDirectiveDecl;
class CommonBlockDecl;
class Sema;

/// BlockStmtBuilder - Constructs bodies for statements and program units.
class BlockStmtBuilder {
public:
  /// \brief A list of executable statements for all the blocks
  std::vector<Stmt*> StmtList;

  /// \brief Represents a statement or a declaration with body(bodies) like DO or IF
  struct Entry {
    Stmt *Statement;
    size_t BeginOffset;
    /// \brief used only when a statement is a do which terminates
    /// with a labeled statement.
    Expr *ExpectedEndDoLabel;

    Entry()
      : Statement(nullptr),BeginOffset(0),
        ExpectedEndDoLabel(nullptr){
    }
    Entry(CFBlockStmt *S)
      : Statement(S), BeginOffset(0),
        ExpectedEndDoLabel(nullptr) {
    }
    Entry(DoStmt *S, Expr *ExpectedEndDo)
      : Statement(S), BeginOffset(0),
        ExpectedEndDoLabel(ExpectedEndDo) {
    }
    Entry(IfStmt *S)
      : Statement(S), BeginOffset(0) {
    }
    Entry(WhereStmt *S)
      : Statement(S), BeginOffset(0) {
    }

    bool hasExpectedDoLabel() const {
      return ExpectedEndDoLabel != nullptr;
    }
  };

  /// \brief A stack of current block statements like IF and DO
  SmallVector<Entry, 16> ControlFlowStack;

  BlockStmtBuilder() {}

  void Enter(Entry S);
  void LeaveIfThen(ASTContext &C);
  void LeaveWhereThen(ASTContext &C);
  void Leave(ASTContext &C);
  Stmt *LeaveOuterBody(ASTContext &C, SourceLocation Loc);

  ArrayRef<Stmt*> getDeclStatements() {
    return StmtList;
  }

  const Entry &LastEntered() const {
    return ControlFlowStack.back();
  }
  bool HasEntered() const {
    return ControlFlowStack.size() != 0;
  }

  void Append(Stmt *S);
private:
  Stmt *CreateBody(ASTContext &C, const Entry &Last);
};

/// StatementLabelScope - This is a component of a scope which assist with
/// declaring and resolving statement labels.
///
class StmtLabelScope {
  StmtLabelScope *Parent;
public:

  /// \brief Represents a usage of an undeclared statement label in
  /// some statement.
  struct ForwardDecl {
    Expr *StmtLabel;
    Stmt *Statement;
    FormatSpec *FS;
    size_t ResolveCallbackData;

    ForwardDecl(Expr *SLabel, Stmt *S,
                size_t CallbackData = 0)
      : StmtLabel(SLabel), Statement(S), FS(nullptr),
        ResolveCallbackData(CallbackData) {
    }

    ForwardDecl(Expr *SLabel, FormatSpec *fs,
                size_t CallbackData = 0)
      : StmtLabel(SLabel), Statement(nullptr), FS(fs),
        ResolveCallbackData(CallbackData) {
    }
  };

private:
  typedef std::map<StmtLabelInteger, Stmt*> StmtLabelMapTy;

  /// StmtLabelDeclsInScope - This keeps track of all the declarations of
  /// statement labels in this scope.
  StmtLabelMapTy StmtLabelDeclsInScope;

  /// ForwardStmtLabelDeclsInScope - This keeps track of all the forward
  /// referenced statement labels in this scope.
  llvm::SmallVector<ForwardDecl, 16> ForwardStmtLabelDeclsInScope;
public:
  StmtLabelScope() : Parent(nullptr) {}

  typedef StmtLabelMapTy::const_iterator decl_iterator;
  decl_iterator decl_begin() const { return StmtLabelDeclsInScope.begin(); }
  decl_iterator decl_end()   const { return StmtLabelDeclsInScope.end(); }
  bool decl_empty()          const { return StmtLabelDeclsInScope.empty(); }

  ArrayRef<ForwardDecl> getForwardDecls() const {
    return ForwardStmtLabelDeclsInScope;
  }

  StmtLabelScope *getParent() const {
    return Parent;
  }
  void setParent(StmtLabelScope *P);

  /// \brief Declares a new statement label.
  void Declare(Expr *StmtLabel, Stmt *Statement);

  /// \brief Tries to resolve a statement label reference.
  /// If the reference is resolved, the statement with the label
  /// is notified that the label is used.
  Stmt *Resolve(Expr *StmtLabel) const;

  /// \brief Declares a forward reference of some statement label.
  void DeclareForwardReference(ForwardDecl Reference);

  /// \brief Removes a forward reference of some statement label.
  void RemoveForwardReference(const Stmt *User);

  /// \brief Returns true is the two statement labels are identical.
  bool IsSame(const Expr *StmtLabelA, const Expr *StmtLabelB) const;
};

/// ConstructNameScope - This is a component of a scope which assists with
/// declaring and resolving construct names.
/// NB: the parent construct name scopes aren't searched when resolving.
///
class ConstructNameScope {
  ConstructNameScope *Parent;
  llvm::DenseMap<const IdentifierInfo*, NamedConstructStmt*> Constructs;

public:

  ConstructNameScope *getParent() const {
    return Parent;
  }
  void setParent(ConstructNameScope *P);

  /// \brief Declares a new construct name.
  /// Returns true if such declaration already exits.
  void Declare(const IdentifierInfo *Name, NamedConstructStmt *Construct);

  /// \brief Tries to resolve a construct name reference.
  /// Returns null if the name wasn't declared.
  NamedConstructStmt *Resolve(const IdentifierInfo *ConstructName) const;
};

/// ImplicitTypingScope - This is a component of a scope which assist with
/// declaring and resolving typing rules using the IMPLICIT statement.
///
class ImplicitTypingScope {
  ImplicitTypingScope *Parent;
  llvm::StringMap<QualType> Rules;
  bool None;
public:
  ImplicitTypingScope(ImplicitTypingScope *Prev = nullptr);

  enum RuleType {
    DefaultRule,
    TypeRule,
    NoneRule
  };

  ImplicitTypingScope *getParent() const {
    return Parent;
  }
  void setParent(ImplicitTypingScope *P);

  /// \brief Associates a type rule with an identifier
  /// returns true if associating is sucessfull.
  bool Apply(const ImplicitStmt::LetterSpecTy& Spec, QualType T);

  /// \brief Applies an IMPLICIT NONE rule.
  /// returns true if the applicating is sucessfull.
  bool ApplyNone();

  /// \brief returns true if IMPLICIT NONE was used in this scope.
  bool isNoneInThisScope() const {
    return None;
  }

  /// \brief Returns a rule and possibly a type associated with this identifier.
  std::pair<RuleType, QualType> Resolve(const IdentifierInfo *IdInfo);
};

/// InnerScope - This is a scope which assists with resolving identifiers
/// in the inner scopes of declaration contexts, such as implied do
/// and statement function.
///
class InnerScope {
  llvm::StringMap<Decl*> Declarations;
  InnerScope *Parent;
public:
  InnerScope(InnerScope *Prev = nullptr);

  InnerScope *getParent() const { return Parent; }

  /// Declares a new declaration in this scope.
  void Declare(const IdentifierInfo *IDInfo, Decl *Declaration);

  /// Returns a valid declaration if such declaration exists in this scope.
  Decl *Lookup(const IdentifierInfo *IDInfo) const;

  /// Resolves an identifier by looking at this and parent scopes.
  Decl *Resolve(const IdentifierInfo *IDInfo) const;
};

/// EquivalenceScope - This is a component of a scope which assists with
/// semantic analysis for the EQUIVALENCE statement and storage unit
/// association for the variables that are influenced by the EQUIVALENCE
/// statement.
///
class EquivalenceScope {
public:
  struct InfluenceObject;

  class Object {
  public:
    const Expr *E;
    uint64_t Offset;
    InfluenceObject *Obj;

    Object(){}
    Object(const Expr *e, uint64_t offset,
           InfluenceObject *obj)
      : E(e), Offset(offset), Obj(obj) {}
  };

  class Connection {
  public:
    Object A;
    Object B;

    Connection(Object a, Object b)
      : A(a), B(b) {}
  };

  class InfluenceObject {
  public:
    VarDecl *Var;
  };
private:
  SmallVector<Connection, 16> Connections;
  llvm::SmallDenseMap<const VarDecl*, InfluenceObject*> Objects;

  InfluenceObject *GetObject(ASTContext &C, VarDecl *Var);
public:

  Object GetObject(ASTContext &C, const Expr *E, VarDecl *Var, uint64_t Offset);

  /// \brief Returns true if the connection between two objects is valid.
  bool CheckConnection(DiagnosticsEngine &Diags, Object A, Object B, bool ReportWarnings = true);

  /// \brief Connects two objects.
  void Connect(Object A, Object B);

  /// \brief Creates the required equivalence sets and associates them with
  /// the influenced objects.
  void CreateEquivalenceSets(ASTContext &C);
};

/// CommonBlockScope - This is a component of a scope which assists with
/// semantic analysis for the COMMON statement and storage unit
/// association for the variables that are influenced by the COMMON
/// statement.
///
class CommonBlockScope {
public:
  typedef llvm::SmallDenseMap<const IdentifierInfo*, CommonBlockDecl*> BlockMappingTy;
private:
  CommonBlockDecl *UnnamedBlock;
  BlockMappingTy Blocks;
public:

  CommonBlockScope();

  bool hasUnnamed() const {
    return UnnamedBlock != nullptr;
  }
  CommonBlockDecl *getUnnamed() const {
    return UnnamedBlock;
  }

  BlockMappingTy::const_iterator beginNamed() const {
    return Blocks.begin();
  }
  BlockMappingTy::const_iterator endNamed() const {
    return Blocks.end();
  }

  CommonBlockDecl *find(const IdentifierInfo *IDInfo);

  CommonBlockDecl *findOrInsert(ASTContext &C, DeclContext *DC,
                                SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo);
};

class CommonBlockSetBuilder;

/// The scope which helps to apply specification statements
class SpecificationScope {

  struct StoredDimensionSpec {
    SourceLocation Loc, IDLoc;
    const IdentifierInfo *IDInfo;
    unsigned Offset, Size;
  };
  SmallVector<StoredDimensionSpec, 8> DimensionSpecs;
  SmallVector<ArraySpec*, 32> Dimensions;

  struct StoredSaveSpec {
    SourceLocation Loc, IDLoc;
    const IdentifierInfo *IDInfo;
  };

  SmallVector<StoredSaveSpec, 4> SaveSpecs;

  struct StoredCommonSpec {
    SourceLocation Loc, IDLoc;
    const IdentifierInfo *IDInfo;
    CommonBlockDecl *Block;
  };
  struct StoredSaveCommonBlockSpec {
    SourceLocation Loc, IDLoc;
    CommonBlockDecl *Block;
  };

  SmallVector<StoredCommonSpec, 4> CommonSpecs;
  SmallVector<StoredSaveCommonBlockSpec, 4> SaveCommonBlockSpecs;

public:

  void AddDimensionSpec(SourceLocation Loc, SourceLocation IDLoc,
                        const IdentifierInfo *IDInfo,
                        ArrayRef<ArraySpec*> Dims);

  bool IsDimensionAppliedTo(const IdentifierInfo *IDInfo) const;

  void ApplyDimensionSpecs(Sema &Visitor);

  void AddSaveSpec(SourceLocation Loc, SourceLocation IDLoc,
                   const IdentifierInfo *IDInfo = nullptr);

  void AddSaveSpec(SourceLocation Loc, SourceLocation IDLoc,
                   CommonBlockDecl *Block);

  void ApplySaveSpecs(Sema &Visitor);

  void AddCommonSpec(SourceLocation Loc, SourceLocation IDLoc,
                     const IdentifierInfo *IDInfo,
                     CommonBlockDecl *Block);

  void ApplyCommonSpecs(Sema &Visitor,
                        CommonBlockSetBuilder &Builder);
};

/// The scope of a translation unit (a single file)
class TranslationUnitScope {
public:
  StmtLabelScope StmtLabels;
  ImplicitTypingScope ImplicitTypingRules;
};

/// The scope of an executable program unit.
class ExecutableProgramUnitScope {
public:
  StmtLabelScope StmtLabels;
  ConstructNameScope NamedConstructs;
  ImplicitTypingScope ImplicitTypingRules;
  EquivalenceScope EquivalenceAssociations;
  CommonBlockScope CommonBlocks;
  BlockStmtBuilder Body;
  SpecificationScope Specs;
};

/// The scope of a main program
class MainProgramScope : public ExecutableProgramUnitScope {
};

/// The scope of a function/subroutine
class SubProgramScope : public ExecutableProgramUnitScope {
};

}  // end namespace flang

#endif
