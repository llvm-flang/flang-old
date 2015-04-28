//===- Scope.cpp - Lexical scope information ------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Scope class, which is used for recording information
// about a lexical scope.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Scope.h"
#include "flang/AST/Expr.h"
#include "flang/AST/StorageSet.h"
#include <limits>

namespace flang {

static StmtLabelInteger GetStmtLabelValue(const Expr *E) {
  if(const IntegerConstantExpr *IExpr =
     dyn_cast<IntegerConstantExpr>(E)) {
    return StmtLabelInteger(
          IExpr->getValue().getLimitedValue(
            std::numeric_limits<StmtLabelInteger>::max()));
  } else {
    llvm_unreachable("Invalid stmt label expression");
    return 0;
  }
}

void StmtLabelScope::setParent(StmtLabelScope *P) {
  assert(!Parent);
  Parent = P;
}

/// \brief Declares a new statement label.
void StmtLabelScope::Declare(Expr *StmtLabel, Stmt *Statement) {
  auto Key = GetStmtLabelValue(StmtLabel);
  StmtLabelDeclsInScope.insert(std::make_pair(Key,Statement));
}

/// \brief Tries to resolve a statement label reference.
Stmt *StmtLabelScope::Resolve(Expr *StmtLabel) const {
  auto Key = GetStmtLabelValue(StmtLabel);
  auto Result = StmtLabelDeclsInScope.find(Key);
  if(Result == StmtLabelDeclsInScope.end()) return nullptr;
  Result->second->setStmtLabelUsed();
  return Result->second;
}

/// \brief Declares a forward reference of some statement label.
void StmtLabelScope::DeclareForwardReference(ForwardDecl Reference) {
  ForwardStmtLabelDeclsInScope.append(1,Reference);
}

/// \brief Removes a forward reference of some statement label.
void StmtLabelScope::RemoveForwardReference(const Stmt *User) {
  for(size_t I = 0; I < ForwardStmtLabelDeclsInScope.size(); ++I) {
    if(ForwardStmtLabelDeclsInScope[I].Statement == User) {
      ForwardStmtLabelDeclsInScope.erase(ForwardStmtLabelDeclsInScope.begin() + I);
      return;
    }
  }
}

/// \brief Returns true is the two statement labels are identical.
bool StmtLabelScope::IsSame(const Expr *StmtLabelA,
                            const Expr *StmtLabelB) const {
  return GetStmtLabelValue(StmtLabelA) == GetStmtLabelValue(StmtLabelB);
}

void ConstructNameScope::setParent(ConstructNameScope *P) {
  Parent = P;
}

void ConstructNameScope::Declare(const IdentifierInfo *Name, NamedConstructStmt *Construct) {
  Constructs.insert(std::make_pair(Name, Construct));
}

NamedConstructStmt *ConstructNameScope::Resolve(const IdentifierInfo *ConstructName) const {
  auto Result = Constructs.find(ConstructName);
  if(Result == Constructs.end()) return nullptr;
  return Result->second;
}

ImplicitTypingScope::ImplicitTypingScope(ImplicitTypingScope *Prev)
  : Parent(Prev), None(false) {
}

void ImplicitTypingScope::setParent(ImplicitTypingScope *P) {
  assert(!Parent);
  Parent = P;
}

bool ImplicitTypingScope::Apply(const ImplicitStmt::LetterSpecTy &Spec, QualType T) {
  if(None) return false;
  char Low = toupper((Spec.first->getNameStart())[0]);
  if(Spec.second) {
    char High = toupper((Spec.second->getNameStart())[0]);
    for(; Low <= High; ++Low) {
      llvm::StringRef Key(&Low, 1);
      if(Rules.find(Key) != Rules.end())
        return false;
      Rules[Key] = T;
    }
  } else {
    llvm::StringRef Key(&Low, 1);
    if(Rules.find(Key) != Rules.end())
      return false;
    Rules[Key] = T;
  }
  return true;
}

bool ImplicitTypingScope::ApplyNone() {
  if(Rules.size()) return false;
  None = true;
  return true;
}

std::pair<ImplicitTypingScope::RuleType, QualType>
ImplicitTypingScope::Resolve(const IdentifierInfo *IdInfo) {
  if(None)
    return std::make_pair(NoneRule, QualType());
  char C = toupper(IdInfo->getNameStart()[0]);
  auto Result = Rules.find(llvm::StringRef(&C, 1));
  if(Result != Rules.end())
    return std::make_pair(TypeRule, Result->getValue());
  else if(Parent)
    return Parent->Resolve(IdInfo);
  else return std::make_pair(DefaultRule, QualType());
}

InnerScope::InnerScope(InnerScope *Prev)
  : Parent(Prev) {
}

void InnerScope::Declare(const IdentifierInfo *IDInfo, Decl *Declaration) {
  Declarations[IDInfo->getName()] = Declaration;
}

Decl *InnerScope::Lookup(const IdentifierInfo *IDInfo) const {
  auto Result = Declarations.find(IDInfo->getName());
  if(Result != Declarations.end())
    return Result->getValue();
  return nullptr;
}

Decl *InnerScope::Resolve(const IdentifierInfo *IDInfo) const {
  auto Result = Lookup(IDInfo);
  return Result? Result : (Parent? Parent->Resolve(IDInfo) : nullptr);
}

CommonBlockScope::CommonBlockScope()
  : UnnamedBlock(nullptr) {}

CommonBlockDecl *CommonBlockScope::find(const IdentifierInfo *IDInfo) {
  assert(IDInfo);
  auto Result = Blocks.find(IDInfo);
  if(Result != Blocks.end())
    return Result->second;
  return nullptr;
}

CommonBlockDecl *CommonBlockScope::findOrInsert(ASTContext &C, DeclContext *DC,
                                                SourceLocation IDLoc,
                                                const IdentifierInfo *IDInfo) {
  if(!IDInfo) {
    if(!UnnamedBlock) {
      UnnamedBlock = CommonBlockDecl::Create(C, DC, IDLoc, IDInfo);
      auto Set = CommonBlockSet::Create(C, UnnamedBlock);
      UnnamedBlock->setStorageSet(Set);
    }
    return UnnamedBlock;
  }
  auto Result = Blocks.find(IDInfo);
  if(Result != Blocks.end())
    return Result->second;
  auto Block = CommonBlockDecl::Create(C, DC, IDLoc, IDInfo);
  auto Set = CommonBlockSet::Create(C, Block);
  Block->setStorageSet(Set);
  Blocks.insert(std::make_pair(IDInfo, Block));
  return Block;
}

} // end namespace flang
