//===--- Sema.h - Semantic Analysis & AST Building --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the Sema class, which performs semantic analysis and builds
// ASTs.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_SEMA_SEMA_H__
#define FLANG_SEMA_SEMA_H__

#include "flang/Basic/Token.h"
#include "flang/AST/FormatSpec.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/Type.h"
#include "flang/AST/Expr.h"
#include "flang/AST/IOSpec.h"
#include "flang/Sema/Ownership.h"
#include "flang/Sema/Scope.h"
#include "flang/Sema/DeclSpec.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SourceMgr.h"
#include "flang/Basic/LLVM.h"
#include <vector>

namespace flang {

class ASTContext;
class DeclContext;
class DeclSpec;
class DeclarationNameInfo;
class DiagnosticsEngine;
class Expr;
class FormatSpec;
class IdentifierInfo;
class Token;
class VarDecl;

/// Sema - This implements semantic analysis and AST buiding for Fortran.
class Sema {
  Sema(const Sema&);           // DO NOT IMPLEMENT
  void operator=(const Sema&); // DO NOT IMPLEMENT


  /// \brief A statement label scope for the current program unit.
  StmtLabelScope *CurStmtLabelScope;

  /// \brief A named constructs scope for the current program unit.
  ConstructNameScope *CurNamedConstructs;

  /// \brief A class which supports the executable statements in
  /// the current scope.
  BlockStmtBuilder *CurExecutableStmts;

  /// \brief The implicit scope for the current program unit.
  ImplicitTypingScope *CurImplicitTypingScope;

  /// \brief The equivalence scope for the current program unit.
  EquivalenceScope *CurEquivalenceScope;

  /// \brief The common block scope for the current program unit.
  CommonBlockScope *CurCommonBlockScope;

  /// \brief The specification scope for the current program unit.
  SpecificationScope *CurSpecScope;

  /// \brief Represents the do loop variable currently being used.
  SmallVector<const VarExpr*, 8> CurLoopVars;

  /// \brief Marks the variable as used by a loop.
  void AddLoopVar(const VarExpr *Var) {
    CurLoopVars.push_back(Var);
  }

  /// \brief Clears the variable of a used by a loop mark.
  void RemoveLoopVar(const VarExpr *Var) {
    for(auto I = CurLoopVars.begin();I!=CurLoopVars.end();++I) {
      if(*I == Var) {
        CurLoopVars.erase(I);
        return;
      }
    }
  }

  /// \brief The mapping
  intrinsic::FunctionMapping IntrinsicFunctionMapping;

public:
  typedef Expr ExprTy;

  ASTContext &Context;
  DiagnosticsEngine &Diags;

  /// CurContext - This is the current declaration context of parsing.
  DeclContext *CurContext;

  Sema(ASTContext &ctxt, DiagnosticsEngine &Diags);
  ~Sema();

  ASTContext &getContext() { return Context; }

  LangOptions getLangOpts() const {
    return Context.getLangOpts();
  }

  DeclContext *getContainingDC(DeclContext *DC);

  StmtLabelScope *getCurrentStmtLabelScope() const {
    return CurStmtLabelScope;
  }

  ConstructNameScope *getCurrentConstructNameScope() const {
    return CurNamedConstructs;
  }

  ImplicitTypingScope *getCurrentImplicitTypingScope() const {
    return CurImplicitTypingScope;
  }

  EquivalenceScope *getCurrentEquivalenceScope() const {
    return CurEquivalenceScope;
  }

  CommonBlockScope *getCurrentCommonBlockScope() const {
    return CurCommonBlockScope;
  }

  BlockStmtBuilder *getCurrentBody() const {
    return CurExecutableStmts;
  }

  SourceRange getTokenRange(SourceLocation Loc);

  inline ExprResult ExprError() const { return ExprResult(true); }
  inline StmtResult StmtError() const { return StmtResult(true); }

  /// Set the current declaration context until it gets popped.
  void PushDeclContext(DeclContext *DC);
  void PopDeclContext();

  bool IsInsideFunctionOrSubroutine() const;
  FunctionDecl *CurrentContextAsFunction() const;

  void PushExecutableProgramUnit(ExecutableProgramUnitScope &Scope);
  void PopExecutableProgramUnit(SourceLocation Loc);

  void PushProgramUnitScope(ExecutableProgramUnitScope &Scope);
  void PopExecutableProgramUnitScope(SourceLocation Loc);

  void DeclareStatementLabel(Expr *StmtLabel, Stmt *S);
  void CheckStatementLabelEndDo(Expr *StmtLabel, Stmt *S);

  void DeclareConstructName(ConstructName Name, NamedConstructStmt *S);

  /// translation unit actions
  void ActOnTranslationUnit(TranslationUnitScope &Scope);
  void ActOnEndTranslationUnit();

  /// program unit actions
  MainProgramDecl *ActOnMainProgram(ASTContext &C, MainProgramScope &Scope,
                                    const IdentifierInfo *IDInfo, SourceLocation NameLoc);

  void ActOnEndMainProgram(SourceLocation Loc);

  FunctionDecl *ActOnSubProgram(ASTContext &C, SubProgramScope &Scope,
                                bool IsSubRoutine, SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo, DeclSpec &ReturnTypeDecl,
                                int Attr);
  void ActOnRESULT(ASTContext &C, SourceLocation IDLoc,
                   const IdentifierInfo *IDInfo);
  VarDecl *ActOnSubProgramArgument(ASTContext &C, SourceLocation IDLoc,
                                   const IdentifierInfo *IDInfo);
  void ActOnSubProgramStarArgument(ASTContext &C, SourceLocation Loc);
  void ActOnSubProgramArgumentList(ASTContext &C, ArrayRef<VarDecl*> Arguments);
  void ActOnEndSubProgram(ASTContext &C, SourceLocation Loc);

  FunctionDecl *ActOnStatementFunction(ASTContext &C,
                                       SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo);
  VarDecl *ActOnStatementFunctionArgument(ASTContext &C, SourceLocation IDLoc,
                                          const IdentifierInfo *IDInfo);
  void ActOnStatementFunctionBody(SourceLocation Loc, ExprResult Body);
  void ActOnEndStatementFunction(ASTContext &C);

  void ActOnSpecificationPart();

  void ActOnFunctionSpecificationPart();

  VarDecl *GetVariableForSpecification(SourceLocation StmtLoc, const IdentifierInfo *IDInfo,
                                       SourceLocation IDLoc,
                                       bool CanBeArgument = true);

  bool ApplyDimensionSpecification(SourceLocation Loc, SourceLocation IDLoc,
                                   const IdentifierInfo *IDInfo,
                                   ArrayRef<ArraySpec*> Dims);

  bool ApplySaveSpecification(SourceLocation Loc, SourceLocation IDLoc,
                              const IdentifierInfo *IDInfo);

  bool ApplySaveSpecification(SourceLocation Loc, SourceLocation IDLoc, VarDecl *VD);

  bool ApplySaveSpecification(SourceLocation Loc, SourceLocation IDLoc,
                              CommonBlockDecl *Block);

  bool ApplyCommonSpecification(SourceLocation Loc, SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo,
                                CommonBlockDecl *Block,
                                CommonBlockSetBuilder &Builder);

  QualType ActOnTypeName(ASTContext &C, DeclSpec &DS);
  VarDecl *ActOnKindSelector(ASTContext &C, SourceLocation IDLoc,
                             const IdentifierInfo *IDInfo);

  /// ActOnAttrSpec - Helper function that assigns the attribute specification to
  /// the list, but reports an error if that attribute was all ready assigned.
  /// Returns true if the attribute specification wasn't applied.
  bool ActOnAttrSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::AS Val);

  /// ActOnDimensionAttrSpec - returns true if the DIMENSION attribute
  /// specification wasn't applied.
  bool ActOnDimensionAttrSpec(ASTContext &C, SourceLocation Loc,
                              DeclSpec &DS,
                              ArrayRef<ArraySpec*> Dimensions);

  /// ActOnAccessSpec - Helper function that assigns the access specification to
  /// the DeclSpec, but reports an error if that access spec was all ready
  /// assigned.
  bool ActOnAccessSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::AC Val);

  /// ActOnIntentSpec - Helper function that assigns the intent specification to
  /// the DeclSpec, but reports an error if that intent spec was all ready
  /// assigned.
  bool ActOnIntentSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::IS Val);

  /// ActOnObjectArraySpec - returns true if the array specification wasn't
  /// applied.
  bool ActOnObjectArraySpec(ASTContext &C, SourceLocation Loc, DeclSpec &DS,
                            ArrayRef<ArraySpec*> Dimensions);

  /// AtOnTypeDeclSpec - returns true if the derived type specification wasn't
  /// applied.
  void ActOnTypeDeclSpec(ASTContext &C, SourceLocation Loc,
                         const IdentifierInfo *IDInfo, DeclSpec &DS);

  VarDecl *CreateImplicitEntityDecl(ASTContext &C, SourceLocation IDLoc,
                                    const IdentifierInfo *IDInfo);

  Decl *ActOnExternalEntityDecl(ASTContext &C, QualType T,
                                SourceLocation IDLoc, const IdentifierInfo *IDInfo);

  Decl *ActOnIntrinsicEntityDecl(ASTContext &C, QualType T,
                                 SourceLocation IDLoc, const IdentifierInfo *IDInfo);

  Decl *ActOnParameterEntityDecl(ASTContext &C, QualType T,
                                 SourceLocation IDLoc, const IdentifierInfo *IDInfo,
                                 SourceLocation EqualLoc, ExprResult Value);

  Decl *ActOnEntityDecl(ASTContext &C, const QualType &T,
                        SourceLocation IDLoc, const IdentifierInfo *IDInfo);

  Decl *ActOnEntityDecl(ASTContext &C, DeclSpec &DS,
                        SourceLocation IDLoc, const IdentifierInfo *IDInfo);

  QualType ResolveImplicitType(const IdentifierInfo *IDInfo);

  Decl *ActOnImplicitEntityDecl(ASTContext &C, SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo);

  Decl *ActOnImplicitFunctionDecl(ASTContext &C, SourceLocation IDLoc,
                                  const IdentifierInfo *IDInfo);

  Decl *ActOnPossibleImplicitFunctionDecl(ASTContext &C, SourceLocation IDLoc,
                                          const IdentifierInfo *IDInfo,
                                          Decl *PrevDecl);

  bool ApplyImplicitRulesToArgument(VarDecl *Arg,
                                    SourceRange Range = SourceRange());

  /// Returns a declaration which matches the identifier in this context
  Decl *LookupIdentifier(const IdentifierInfo *IDInfo);

  Decl *ResolveIdentifier(const IdentifierInfo *IDInfo);

  /// \brief Returns a variable declaration if the given identifier resolves
  /// to a variable, or null otherwise. If the identifier isn't resolved
  /// an implicit variable declaration will be created whenever possible.
  VarDecl *ExpectVarRefOrDeclImplicitVar(SourceLocation IDLoc,
                                         const IdentifierInfo *IDInfo);

  /// \brief Returns a variable declaration if the given identifier resolves to
  /// a variable, or null otherwise.
  VarDecl *ExpectVarRef(SourceLocation IDLoc,
                        const IdentifierInfo *IDInfo);

  VarExpr *ConstructRecoveryVariable(ASTContext &C, SourceLocation Loc,
                                     QualType T);

  /// \brief Returns true if the given identifier can be used as the function name
  /// in a statement function declaration. This function resolves the ambiguity
  /// of statement function declarations and array subscript assignments.
  bool IsValidStatementFunctionIdentifier(const IdentifierInfo *IDInfo);

  RecordDecl *ActOnDerivedTypeDecl(ASTContext &C, SourceLocation Loc,
                                   SourceLocation IDLoc, const IdentifierInfo* IDInfo);

  void ActOnDerivedTypeSequenceStmt(ASTContext &C, SourceLocation Loc);

  FieldDecl *ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo,
                                       ExprResult Init = ExprResult());

  void ActOnENDTYPE(ASTContext &C, SourceLocation Loc,
                    SourceLocation IDLoc, const IdentifierInfo* IDInfo);

  void ActOnEndDerivedTypeDecl(ASTContext &C);

  StmtResult ActOnCompoundStmt(ASTContext &C, SourceLocation Loc,
                               ArrayRef<Stmt*> Body, Expr *StmtLabel);

  // PROGRAM statement:
  StmtResult ActOnPROGRAM(ASTContext &C, const IdentifierInfo *ProgName,
                          SourceLocation Loc, SourceLocation NameLoc, Expr *StmtLabel);

  // END PROGRAM / SUBROUTINE / FUNCTION statement:
  StmtResult ActOnEND(ASTContext &C, SourceLocation Loc,
                      ConstructPartStmt::ConstructStmtClass Kind,
                      SourceLocation IDLoc, const IdentifierInfo *IDInfo,
                      Expr *StmtLabel);

  // USE statement:
  StmtResult ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                      const IdentifierInfo *ModName, Expr *StmtLabel);
  StmtResult ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                      const IdentifierInfo *ModName, bool OnlyList,
                      ArrayRef<UseStmt::RenamePair> RenameNames,
                      Expr *StmtLabel);

  // IMPORT statement:
  StmtResult ActOnIMPORT(ASTContext &C, SourceLocation Loc,
                         ArrayRef<const IdentifierInfo*> ImportNamesList,
                         Expr *StmtLabel);

  // IMPLICIT statement:
  StmtResult ActOnIMPLICIT(ASTContext &C, SourceLocation Loc, DeclSpec &DS,
                           ImplicitStmt::LetterSpecTy LetterSpec, Expr *StmtLabel);

  StmtResult ActOnIMPLICIT(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  // DIMENSION statement
  // The source code statement is split into multiple ones in the parsing stage.
  StmtResult ActOnDIMENSION(ASTContext &C, SourceLocation Loc, SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo,
                            ArrayRef<ArraySpec*> Dims,
                            Expr *StmtLabel);

  // PARAMETER statement:
  StmtResult ActOnPARAMETER(ASTContext &C, SourceLocation Loc,
                            SourceLocation EqualLoc,
                            SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo,
                            ExprResult Value,
                            Expr *StmtLabel);

  // ASYNCHRONOUS statement:
  StmtResult ActOnASYNCHRONOUS(ASTContext &C, SourceLocation Loc,
                               ArrayRef<const IdentifierInfo*> ObjNames,
                               Expr *StmtLabel);

  // EXTERNAL statement:
  StmtResult ActOnEXTERNAL(ASTContext &C, SourceLocation Loc,
                           SourceLocation IDLoc,
                           const IdentifierInfo *IDInfo,
                           Expr *StmtLabel);

  // INTRINSIC statement:
  StmtResult ActOnINTRINSIC(ASTContext &C, SourceLocation Loc,
                            SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo,
                            Expr *StmtLabel);

  // SAVE statement
  StmtResult ActOnSAVE(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnSAVE(ASTContext &C, SourceLocation Loc,
                       SourceLocation IDLoc,
                       const IdentifierInfo *IDInfo,
                       Expr *StmtLabel);

  StmtResult ActOnSAVECommonBlock(ASTContext &C, SourceLocation Loc,
                                  SourceLocation IDLoc,
                                  const IdentifierInfo *IDInfo);

  // EQUIVALENCE statement
  StmtResult ActOnEQUIVALENCE(ASTContext &C, SourceLocation Loc,
                              SourceLocation PartLoc,
                              ArrayRef<Expr*> ObjectList,
                              Expr *StmtLabel);

  bool CheckEquivalenceObject(SourceLocation Loc, Expr *E, VarDecl *&Object);

  bool CheckEquivalenceType(QualType ExpectedType, const Expr *E);

  void ActOnCOMMON(ASTContext &C, SourceLocation Loc, SourceLocation BlockLoc,
                   SourceLocation IDLoc, const IdentifierInfo *BlockID,
                   const IdentifierInfo *IDInfo, ArrayRef<ArraySpec *> Dimensions);

  // DATA statement:
  StmtResult ActOnDATA(ASTContext &C, SourceLocation Loc,
                       ArrayRef<Expr*> LHS, ArrayRef<Expr*> Values,
                       Expr *StmtLabel);

  ExprResult ActOnDATAConstantExpr(ASTContext &C, SourceLocation RepeatLoc,
                                   ExprResult RepeatCount,
                                   ExprResult Value);

  ExprResult ActOnDATAOuterImpliedDoExpr(ASTContext &C,
                                         ExprResult Expression);

  ExprResult ActOnDATAImpliedDoExpr(ASTContext &C, SourceLocation Loc,
                                    SourceLocation IDLoc,
                                    const IdentifierInfo *IDInfo,
                                    ArrayRef<ExprResult> Body,
                                    ExprResult E1, ExprResult E2,
                                    ExprResult E3);

  StmtResult ActOnAssignmentStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult LHS,
                                 ExprResult RHS, Expr *StmtLabel);

  QualType ActOnArraySpec(ASTContext &C, QualType ElemTy,
                          ArrayRef<ArraySpec *> Dims);

  StarFormatSpec *ActOnStarFormatSpec(ASTContext &C, SourceLocation Loc);
  LabelFormatSpec *ActOnLabelFormatSpec(ASTContext &C, SourceLocation Loc,
                                        ExprResult Label);
  FormatSpec *ActOnExpressionFormatSpec(ASTContext &C, SourceLocation Loc,
                                             Expr *E);

  ExternalStarUnitSpec *ActOnStarUnitSpec(ASTContext &C, SourceLocation Loc,
                                          bool IsLabeled);
  UnitSpec *ActOnUnitSpec(ASTContext &C, ExprResult Value, SourceLocation Loc,
                          bool IsLabeled);

  StmtResult ActOnAssignStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Value, VarExpr* VarRef,
                             Expr *StmtLabel);

  StmtResult ActOnAssignedGotoStmt(ASTContext &C, SourceLocation Loc,
                                   VarExpr* VarRef, ArrayRef<Expr *> AllowedValues,
                                   Expr *StmtLabel);

  StmtResult ActOnGotoStmt(ASTContext &C, SourceLocation Loc,
                           ExprResult Destination, Expr *StmtLabel);

  StmtResult ActOnComputedGotoStmt(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<Expr*> Targets,
                                   ExprResult Operand, Expr *StmtLabel);

  StmtResult ActOnIfStmt(ASTContext &C, SourceLocation Loc,
                         ExprResult Condition, ConstructName Name,
                         Expr *StmtLabel);
  StmtResult ActOnElseIfStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Condition, ConstructName Name, Expr *StmtLabel);
  StmtResult ActOnElseStmt(ASTContext &C, SourceLocation Loc,
                           ConstructName Name, Expr *StmtLabel);
  StmtResult ActOnEndIfStmt(ASTContext &C, SourceLocation Loc,
                            ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnDoStmt(ASTContext &C, SourceLocation Loc, SourceLocation EqualLoc,
                         ExprResult TerminatingStmt,
                         VarExpr *DoVar, ExprResult E1, ExprResult E2,
                         ExprResult E3, ConstructName Name,
                         Expr *StmtLabel);

  StmtResult ActOnDoWhileStmt(ASTContext &C, SourceLocation Loc, ExprResult Condition,
                              ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnEndDoStmt(ASTContext &C, SourceLocation Loc,
                            ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnCycleStmt(ASTContext &C, SourceLocation Loc,
                            ConstructName LoopName, Expr *StmtLabel);

  StmtResult ActOnExitStmt(ASTContext &C, SourceLocation Loc,
                           ConstructName LoopName, Expr *StmtLabel);

  StmtResult ActOnSelectCaseStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Operand,
                                 ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnCaseDefaultStmt(ASTContext &C, SourceLocation Loc,
                                  ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnCaseStmt(ASTContext &C, SourceLocation Loc,
                           llvm::MutableArrayRef<Expr*> Values,
                           ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnEndSelectStmt(ASTContext &C, SourceLocation Loc,
                                ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnWhereStmt(ASTContext &C, SourceLocation Loc,
                            ExprResult Mask, Expr *StmtLabel);

  StmtResult ActOnWhereStmt(ASTContext &C, SourceLocation Loc,
                            ExprResult Mask, StmtResult Body, Expr *StmtLabel);

  StmtResult ActOnElseWhereStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnEndWhereStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnContinueStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnStopStmt(ASTContext &C, SourceLocation Loc, ExprResult StopCode, Expr *StmtLabel);

  StmtResult ActOnReturnStmt(ASTContext &C, SourceLocation Loc, ExprResult E, Expr *StmtLabel);

  StmtResult ActOnCallStmt(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                           SourceLocation IDLoc,
                           const IdentifierInfo *IDInfo,
                           llvm::MutableArrayRef<Expr *> Arguments, Expr *StmtLabel);

  StmtResult ActOnPrintStmt(ASTContext &C, SourceLocation Loc, FormatSpec *FS,
                            ArrayRef<ExprResult> OutputItemList,
                            Expr *StmtLabel);

  StmtResult ActOnWriteStmt(ASTContext &C, SourceLocation Loc,
                            UnitSpec *US, FormatSpec *FS,
                            ArrayRef<ExprResult> OutputItemList,
                            Expr *StmtLabel);

  // FIXME: TODO:

  QualType ActOnBuiltinType(ASTContext *Ctx,
                            BuiltinType::TypeSpec TS,
                            Expr *Kind) { return QualType(); }
  QualType ActOnCharacterBuiltinType(ASTContext *Ctx,
                                     Expr *Len,
                                     Expr *Kind) { return QualType(); }

  ExprResult ActOnDataReference(llvm::ArrayRef<ExprResult> Exprs) {
    return ExprResult();
  }

  ExprResult ActOnComplexConstantExpr(ASTContext &C, SourceLocation Loc,
                                      SourceLocation MaxLoc,
                                      ExprResult RealPart, ExprResult ImPart);

  /// Returns true if a type is a double precision real type.
  bool IsTypeDoublePrecisionReal(QualType T) const;

  /// Returns true if a type is a double precision complex type.
  bool IsTypeDoublePrecisionComplex(QualType T) const;

  /// GetUnaryReturnType - Returns the type T with the
  /// required qualifiers and array type from the given expression.
  QualType GetUnaryReturnType(const Expr *E, QualType T);

  ExprResult ActOnUnaryExpr(ASTContext &C, SourceLocation Loc,
                            UnaryExpr::Operator Op, ExprResult E);

  /// GetBinaryReturnType - Returns the type T with the
  /// required qualifiers and array type from the given expression.
  QualType GetBinaryReturnType(const Expr *LHS, const Expr *RHS,
                               QualType T);

  ExprResult ActOnBinaryExpr(ASTContext &C, SourceLocation Loc,
                             BinaryExpr::Operator Op,
                             ExprResult LHS,ExprResult RHS);

  ExprResult ActOnSubstringExpr(ASTContext &C, SourceLocation Loc,
                                Expr *Target,
                                Expr *StartingPoint, Expr *EndPoint);

  ExprResult ActOnSubscriptExpr(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                                Expr* Target, llvm::ArrayRef<Expr*> Subscripts);

  ExprResult ActOnCallExpr(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                           SourceLocation IDLoc,
                           FunctionDecl *Function, llvm::MutableArrayRef<Expr *> Arguments);

  ExprResult ActOnIntrinsicFunctionCallExpr(ASTContext &C, SourceLocation Loc,
                                            const IntrinsicFunctionDecl *FunctionDecl,
                                            ArrayRef<Expr*> Arguments);

  ExprResult ActOnArrayConstructorExpr(ASTContext &C, SourceLocation Loc,
                                       SourceLocation RParenLoc, ArrayRef<Expr*> Elements);

  ExprResult ActOnTypeConstructorExpr(ASTContext &C, SourceLocation Loc, SourceLocation LParenLoc,
                                      SourceLocation RParenLoc, RecordDecl *Record,
                                      ArrayRef<Expr*> Arguments);


  ExprResult ActOnStructureComponentExpr(ASTContext &C, SourceLocation Loc,
                                         SourceLocation IDLoc,
                                         const IdentifierInfo *IDInfo, Expr *Target);

  /// AssignmentAction - This is used by all the assignment diagnostic functions
  /// to represent what is actually causing the operation
  class AssignmentAction {
  public:
    enum Type {
      Assigning,
      Passing,
      Returning,
      Converting,
      Initializing
    };
  private:
    Type ActTy;
    const Decl *D;
  public:

    AssignmentAction(Type Ty)
      : ActTy(Ty), D(nullptr) {}
    AssignmentAction(Type Ty, const Decl *d)
      : ActTy(Ty), D(d) {}

    Type getType() const  {
      return ActTy;
    }
    const Decl *getDecl() const {
      return D;
    }
  };

  /// AssignConvertType - All of the 'assignment' semantic checks return this
  /// enum to indicate whether the assignment was allowed. These checks are
  /// done for simple assignments, as well as initialization, return from
  /// function, argument passing, etc. The query is phrased in terms of a
  /// source and destination type.
  enum AssignConvertType {
    /// Compatible - the types are compatible according to the standard.
    Compatible,

    /// Incompatible - We reject this conversion outright, it is invalid to
    /// represent it in the AST.
    Incompatible,

    /// IncompatibleDimensions - We reject this conversion outright because
    /// of array dimension incompability, it is invalid to
    /// represent it in the AST.
    IncompatibleDimensions
  };

  /// DiagnoseAssignmentResult - Emit a diagnostic, if required, for the
  /// assignment conversion type specified by ConvTy. This returns true if the
  /// conversion was invalid or false if the conversion was accepted.
  bool DiagnoseAssignmentResult(AssignConvertType ConvTy,
                                SourceLocation Loc,
                                QualType DstType, QualType SrcType,
                                const Expr *SrcExpr, AssignmentAction Action,
                                const Expr *DstExpr = nullptr);

  ExprResult
  CheckAndApplyAssignmentConstraints(SourceLocation Loc, QualType LHSType,
                                     Expr *RHS,
                                     AssignmentAction AAction,
                                     const Expr *LHS = nullptr);


  // Format
  StmtResult ActOnFORMAT(ASTContext &C, SourceLocation Loc,
                         FormatItemResult Items,
                         FormatItemResult UnlimitedItems,
                         Expr *StmtLabel, bool IsInline = false);

  FormatItemResult ActOnFORMATIntegerDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                  tok::TokenKind Kind,
                                                  IntegerConstantExpr *RepeatCount,
                                                  IntegerConstantExpr *W,
                                                  IntegerConstantExpr *M);

  FormatItemResult ActOnFORMATRealDataEditDesc(ASTContext &C, SourceLocation Loc,
                                               tok::TokenKind Kind,
                                               IntegerConstantExpr *RepeatCount,
                                               IntegerConstantExpr *W,
                                               IntegerConstantExpr *D,
                                               IntegerConstantExpr *E);

  FormatItemResult ActOnFORMATLogicalDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                  tok::TokenKind Kind,
                                                  IntegerConstantExpr *RepeatCount,
                                                  IntegerConstantExpr *W);

  FormatItemResult ActOnFORMATCharacterDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                    tok::TokenKind Kind,
                                                    IntegerConstantExpr *RepeatCount,
                                                    IntegerConstantExpr *W);

  FormatItemResult ActOnFORMATPositionEditDesc(ASTContext &C, SourceLocation Loc,
                                               tok::TokenKind Kind,
                                               IntegerConstantExpr *N);

  FormatItemResult ActOnFORMATControlEditDesc(ASTContext &C, SourceLocation Loc,
                                              tok::TokenKind Kind);

  FormatItemResult ActOnFORMATCharacterStringDesc(ASTContext &C, SourceLocation Loc,
                                                  ExprResult E);

  FormatItemResult ActOnFORMATFormatItemList(ASTContext &C, SourceLocation Loc,
                                             IntegerConstantExpr *RepeatCount,
                                             ArrayRef<FormatItem*> Items);

  /// Returns true if the declaration with the given name is valid.
  bool CheckDeclaration(const IdentifierInfo *IDInfo, SourceLocation IDLoc);

  void DiagnoseRedefinition(SourceLocation Loc, const IdentifierInfo *IDInfo,
                            Decl *Prev);

  /// Returns evaluated integer,
  /// or an ErrorValue if the expression couldn't
  /// be evaluated.
  int64_t EvalAndCheckIntExpr(const Expr *E,
                              int64_t ErrorValue);

  /// Checks if an evaluated integer greater than 0.
  /// Returns EvalResult if EvalResult > 0, or the error
  /// value if EvalResult <= 0
  int64_t CheckIntGT0(const Expr *E, int64_t EvalResult, int64_t ErrorValue = 1);

  /// Returns evaluated kind specification for the builtin types.
  BuiltinType::TypeKind EvalAndCheckTypeKind(QualType T,
                                             const Expr *E);

  QualType ApplyTypeKind(QualType T, const Expr *E);

  /// Returns evaluated length specification
  /// fot the character type.
  uint64_t EvalAndCheckCharacterLength(const Expr *E);

  /// Returns true if an expression is constant(i.e. evaluatable)
  bool CheckConstantExpression(const Expr *E);

  /// Returns true if an expression is a real or integer constant expression.
  bool CheckIntegerOrRealConstantExpression(const Expr *E);

  /// Returns true if an expression is dependent on a
  /// function argument. Does integer typechecking and
  /// reports argument type errors.
  bool CheckArgumentDependentEvaluatableIntegerExpression(Expr *E);

  /// Returns true if an expression is constant
  bool StatementRequiresConstantExpression(SourceLocation Loc, const Expr *E);

  /// Returns true if an expression is an integer expression
  bool CheckIntegerExpression(const Expr *E);

  /// Returns true if an expression is an integer expression
  bool StmtRequiresIntegerExpression(SourceLocation Loc, const Expr *E);

  /// Returns true if an expression is a scalar numeric expression
  bool CheckScalarNumericExpression(const Expr *E);

  /// Returns true if a variable reference points to an integer
  /// variable
  bool StmtRequiresIntegerVar(SourceLocation Loc, const VarExpr *E);

  /// Returns true if a variable reference points to an integer
  /// or a real variable
  bool StmtRequiresScalarNumericVar(SourceLocation Loc, const VarExpr *E, unsigned DiagId);

  /// Returns true if an expression is a logical expression
  bool CheckLogicalExpression(const Expr *E);

  /// Returns true if an expression is a logical expression
  bool StmtRequiresLogicalExpression(SourceLocation Loc, const Expr *E);

  /// Returns true if an expression is a logical array expression
  bool StmtRequiresLogicalArrayExpression(SourceLocation Loc, const Expr *E);

  /// Returns true if an expression is an integer, logical or a character expression.
  bool StmtRequiresIntegerOrLogicalOrCharacterExpression(SourceLocation Loc, const Expr *E);

  /// Returns true if an expression is a character expression
  bool CheckCharacterExpression(const Expr *E);

  /// Returns true if two types have the same type class
  /// and kind.
  bool AreTypesOfSameKind(QualType A, QualType B) const;

  /// Returns true if two types have the same type class
  /// and kind.
  bool CheckTypesOfSameKind(QualType A, QualType B,
                            const Expr *E) const;

  /// Returns true if the given Type is a scalar(integer,
  /// real, complex) or character
  bool CheckTypeScalarOrCharacter(const Expr *E, QualType T,
                                  bool IsConstant = false);

  /// Typechecks the expression - the following rules are correct:
  /// if the expected type is integer and expression is integer(any kind)
  /// if the expected type is logical and expression is logical(any kind)
  /// if the expected type is character and expression is character(same kind)
  Expr *TypecheckExprIntegerOrLogicalOrSameCharacter(Expr *E,
                                                     QualType ExpectedType);

  /// Returns true if the type is of default kind,
  /// or is a double precision type
  bool IsDefaultBuiltinOrDoublePrecisionType(QualType T);

  /// Returns true if the expession's type is of default kind,
  /// or is a double precision type
  bool CheckDefaultBuiltinOrDoublePrecisionExpression(const Expr *E);

  /// Checks that all of the expressions have the same type
  /// class and kind.
  void CheckExpressionListSameTypeKind(ArrayRef<Expr*> Expressions, bool AllowArrays = false);

  /// Returns true if the argument count doesn't match to the function
  /// count
  bool CheckIntrinsicCallArgumentCount(intrinsic::FunctionKind Function,
                                       ArrayRef<Expr*> Args, SourceLocation Loc);

  /// Returns false if the call to a function from a conversion group
  /// is valid.
  bool CheckIntrinsicConversionFunc(intrinsic::FunctionKind Function,
                                    ArrayRef<Expr*> Args,
                                    QualType &ReturnType);

  /// Returns false if the call to a function from the truncation group
  /// is valid.
  bool CheckIntrinsicTruncationFunc(intrinsic::FunctionKind Function,
                                    ArrayRef<Expr*> Args,
                                    QualType &ReturnType);

  /// Returns false if the call to a function from the complex group
  /// is valid.
  bool CheckIntrinsicComplexFunc(intrinsic::FunctionKind Function,
                                 ArrayRef<Expr*> Args,
                                 QualType &ReturnType);

  /// Returns false if the call to a function from the maths group
  /// is valid.
  bool CheckIntrinsicMathsFunc(intrinsic::FunctionKind Function,
                               ArrayRef<Expr*> Args,
                               QualType &ReturnType);

  /// Returns false if the call to a function from the character group
  /// is valid.
  bool CheckIntrinsicCharacterFunc(intrinsic::FunctionKind Function,
                                   ArrayRef<Expr*> Args,
                                   QualType &ReturnType);

  /// Returns false if the call to a function from the array group
  /// is valid.
  bool CheckIntrinsicArrayFunc(intrinsic::FunctionKind Function,
                               ArrayRef<Expr*> Args,
                               QualType &ReturnType);

  /// Returns false if the call to a function from the numeric inquiry group
  /// is valid.
  bool CheckIntrinsicNumericInquiryFunc(intrinsic::FunctionKind Function,
                                        ArrayRef<Expr*> Args,
                                        QualType &ReturnType);

  /// Returns false if the call to a function from the system group
  /// is valid.
  bool CheckIntrinsicSystemFunc(intrinsic::FunctionKind Function,
                                ArrayRef<Expr*> Args,
                                QualType &ReturnType);

  /// Returns false if the call to a function from the inquiry group
  /// is valid.
  bool CheckIntrinsicInquiryFunc(intrinsic::FunctionKind Function,
                                 ArrayRef<Expr*> Args,
                                 QualType &ReturnType);

  /// Returns false if the call to a function from the
  /// bit operations group is valid.
  bool CheckIntrinsicBitFunc(intrinsic::FunctionKind Function,
                             ArrayRef<Expr*> Args,
                             QualType &ReturnType);

  /// Reports an incompatible argument error and returns true.
  bool DiagnoseIncompatiblePassing(const Expr *E, QualType T,
                                   bool AllowArrays,
                                   StringRef ArgName = StringRef());
  bool DiagnoseIncompatiblePassing(const Expr *E, StringRef T,
                                   bool AllowArrays,
                                   StringRef ArgName = StringRef());

  bool CheckArgumentsTypeCompability(const Expr *E1, const Expr *E2,
                                     StringRef ArgName1, StringRef ArgName2,
                                     bool AllowArrays = false);

  /// Returns false if the argument's type is built in.
  bool CheckBuiltinTypeArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument's type is integer.
  bool CheckIntegerArgument(const Expr *E, bool AllowArrays = false,
                            StringRef ArgName = StringRef());

  /// Returns false if the argument's type is real.
  bool CheckRealArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument's type is complex.
  bool CheckComplexArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument's type is real but isn't double precision.
  bool CheckStrictlyRealArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument's type is real array.
  bool CheckStrictlyRealArrayArgument(const Expr *E, StringRef ArgName);

  /// Returns false if the argument's type is real and is double precision.
  bool CheckDoublePrecisionRealArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument's type is complex and is double complex.
  bool CheckDoubleComplexArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument's type is character.
  bool CheckCharacterArgument(const Expr *E);

  /// Returns false if the argument has an integer or a real type.
  bool CheckIntegerOrRealArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument has an integer or a real array type.
  bool CheckIntegerOrRealArrayArgument(const Expr *E, StringRef ArgName);

  /// Returns false if the argument has an integer or a real or
  /// a complex argument.
  bool CheckIntegerOrRealOrComplexArgument(const Expr *E, bool AllowArrays = false);

  /// Returns false if the argument has a real or
  /// a complex argument.
  bool CheckRealOrComplexArgument(const Expr *E, bool AllowArrays = false);

  /// Returns true if the given expression is a logical array.
  bool IsLogicalArray(const Expr *E);

  /// Returns false if the argument has a logical array type.
  bool CheckLogicalArrayArgument(const Expr *E, StringRef ArgName);

  /// Returns false if the argument is an integer or a logical array.
  bool CheckIntegerArgumentOrLogicalArrayArgument(const Expr *E, StringRef ArgName1,
                                                  StringRef ArgName2);

  /// Returns false if the array argument is compatible with a given array type.
  bool CheckArrayArgumentDimensionCompability(const Expr *E, const ArrayType *AT,
                                              StringRef ArgName);

  /// Returns false if the two array arguments are compatible with each other
  bool CheckArrayArgumentsDimensionCompability(const Expr *E1, const Expr *E2,
                                               StringRef ArgName1, StringRef ArgName2);

  bool IsValidFunctionType(QualType Type);

  /// Sets a type for a function
  void SetFunctionType(FunctionDecl *Function, QualType Type,
                       SourceLocation DiagLoc, SourceRange DiagRange);

  /// Returns true if the call expression has the correct arguments.
  bool CheckCallArguments(FunctionDecl *Function, llvm::MutableArrayRef<Expr *> Arguments,
                          SourceLocation Loc, SourceLocation IDLoc);

  /// Returns an array that is passed to a function, with optional implcit operations.
  Expr *ActOnArrayArgument(VarDecl *Arg, Expr *E);

  /// Returns true if an array expression needs a temporary storage array.
  bool ArrayExprNeedsTemp(const Expr *E);

  /// Returns true if the array shape bound is valid
  bool CheckArrayBoundValue(Expr *E);

  /// Returns true if the given array type can be applied to a declaration.
  bool CheckArrayTypeDeclarationCompability(const ArrayType *T, VarDecl *VD);

  /// Returns true if the given character length can be applied to a declaration.
  bool CheckCharacterLengthDeclarationCompability(QualType T, VarDecl *VD);

  /// Returns true if the subscript expression has the
  /// right amount of dimensions.
  bool CheckSubscriptExprDimensionCount(SourceLocation Loc, SourceLocation RParenLoc,
                                        Expr *Target,
                                        ArrayRef<Expr *> Arguments);

  /// Returns true if the items in the array constructor
  /// satisfy all the constraints.
  /// As a bonus it also returns the Element type in ObtainedElementType.
  bool CheckArrayConstructorItems(ArrayRef<Expr*> Items,
                                  QualType &ResultingArrayType);


  /// Returns true if an array doesn't
  /// have an implied shape dimension specifier.
  bool CheckArrayNoImpliedDimension(const ArrayType *T,
                                    SourceRange Range);

  /// Returns true if an array expression is usable for a unary expression.
  bool CheckArrayExpr(const Expr *E);

  /// Returns true if the two array types are compatible with
  /// one another, i.e. they have the same dimension count
  /// and the shapes of the dimensions are identical
  bool CheckArrayDimensionsCompability(const ArrayType *LHS,
                                       const ArrayType *RHS, SourceLocation Loc,
                                       SourceRange LHSRange, SourceRange RHSRange);

  /// Returns true if the variable can be assigned to (mutated)
  bool CheckVarIsAssignable(const VarExpr *E);

  /// Returns true if a statement is a valid do terminator
  bool IsValidDoTerminatingStatement(const Stmt *S);

  /// Reports an unterminated construct such as do, if, etc.
  void ReportUnterminatedStmt(const BlockStmtBuilder::Entry &S,
                              SourceLocation Loc,
                              bool ReportUnterminatedLabeledDo = true);

  /// Leaves the last block construct, and performs any clean up
  /// that might be needed.
  void LeaveLastBlock();

  /// Leaves the block constructs until the given construct is reached.
  void LeaveUnterminatedBlocksUntil(SourceLocation Loc, Stmt *S);

  /// Leaves block constructs until a do construct is reached.
  /// NB: the do statements with a termination label such as DO 100 I = ..
  /// are popped.
  Stmt *LeaveBlocksUntilDo(SourceLocation Loc);

  /// Returns true if the current statement is inside a do construct which
  /// is terminated by the given statement label.
  bool IsInLabeledDo(const Expr *StmtLabel);

  /// Leaves block constructs until a label termination do construct is reached.
  DoStmt *LeaveBlocksUntilLabeledDo(SourceLocation Loc, const Expr *StmtLabel);

  /// Leaves block constructs until an if construct is reached.
  IfStmt *LeaveBlocksUntilIf(SourceLocation Loc);

  /// Leaves block constructs until a select case construct is reached.
  SelectCaseStmt *LeaveBlocksUntilSelectCase(SourceLocation Loc);

  /// Leaves block constructs until a where construct is reached.
  WhereStmt *LeaveBlocksUntilWhere(SourceLocation Loc);

  /// Returns the amount of where constructs that were entered.
  int CountWhereConstructs();

  /// Checks to see if the part of the constructs has a valid construct name.
  void CheckConstructNameMatch(Stmt *Part, ConstructName Name, Stmt *S);

  /// Checks to see if a statement is inside an outer loop or a loop
  /// associated with a given name, and returns this loop. Null
  /// is returned when an error occurs.
  Stmt *CheckWithinLoopRange(const char *StmtString, SourceLocation Loc, ConstructName Name);

  /// Returns true if we are inside a where construct.
  bool InsideWhereConstruct(Stmt *S);

  /// Returns true if the given statement can be part of a where construct.
  bool CheckValidWhereStmtPart(Stmt *S);

  /// Returns true if the current function/subroutine is recursive.
  bool CheckRecursiveFunction(SourceLocation Loc);

  /// Returns a vector of elements with a given size.
  QualType GetSingleDimArrayType(QualType ElTy, int Size);

};

} // end flang namespace

#endif
