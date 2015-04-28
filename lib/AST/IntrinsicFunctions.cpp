//===--- IntrinsicFunctions.cpp - Intrinsic Function Kinds Support -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the FunctionKind enum and support functions.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/IntrinsicFunctions.h"
#include <cassert>

namespace flang {
namespace intrinsic {

static char FunctionNames[][NUM_FUNCTIONS] = {
#define INTRINSIC_FUNCTION(NAME, GENERICNAME, NUMARGS, VERSION) #NAME,
#include "flang/AST/IntrinsicFunctions.def"
  "\0"
};

static void InitFunctionNames() {
  for(unsigned I = 0; I < NUM_FUNCTIONS; ++I) {
    for(auto Str = FunctionNames[I]; Str[0] != '\0'; ++Str)
      Str[0] = ::tolower(Str[0]);
  }
}

const char *getFunctionName(FunctionKind Kind) {
  assert(Kind < NUM_FUNCTIONS && "Invalid function kind!");
  return FunctionNames[Kind];
}

static FunctionKind FunctionGenericKinds[] = {
  #define INTRINSIC_FUNCTION(NAME, GENERICNAME, NUMARGS, VERSION) GENERICNAME,
  #include "flang/AST/IntrinsicFunctions.def"
};

FunctionKind getGenericFunctionKind(FunctionKind Function) {
  return FunctionGenericKinds[Function];
}

static Group FunctionGroups[] = {
  #define INTRINSIC_FUNCTION(NAME, GENERICNAME, NUMARGS, VERSION) GROUP_NONE,
  #include "flang/AST/IntrinsicFunctions.def"
};

Group getFunctionGroup(FunctionKind Function) {
  return FunctionGroups[Function];
}

static void InitFunctionGroups() {
  #define INTRINSIC_GROUP(NAME, FIRST, LAST) \
    for(size_t I = FIRST; I <= LAST; ++I)    \
      FunctionGroups[I] = GROUP_ ## NAME;
  #include "flang/AST/IntrinsicFunctions.def"
}

static FunctionArgumentCountKind FunctionArgCounts[] = {
  #define NUM_ARGS_1 ArgumentCount1
  #define NUM_ARGS_2 ArgumentCount2
  #define NUM_ARGS_3 ArgumentCount3
  #define NUM_ARGS_1_OR_2 ArgumentCount1or2
  #define NUM_ARGS_2_OR_MORE ArgumentCount2orMore
  #define INTRINSIC_FUNCTION(NAME, GENERICNAME, NUMARGS, VERSION) NUMARGS,
  #include "flang/AST/IntrinsicFunctions.def"
    ArgumentCount1,
};

FunctionArgumentCountKind getFunctionArgumentCount(FunctionKind Function) {
  assert(Function < NUM_FUNCTIONS && "Invalid function kind!");
  return FunctionArgCounts[Function];
}

FunctionMapping::FunctionMapping(const LangOptions &) {
  InitFunctionNames();
  for(unsigned I = 0; I < NUM_FUNCTIONS; ++I) {
    Mapping[getFunctionName(FunctionKind(I))] = FunctionKind(I);
  }
  InitFunctionGroups();
}

FunctionMapping::Result FunctionMapping::Resolve(const IdentifierInfo *IDInfo) {
  auto It = Mapping.find(IDInfo->getName());
  if(It == Mapping.end()) {
    Result Res = { NUM_FUNCTIONS, true };
    return Res;
  }
  Result Res = { It->getValue(), false };
  return Res;
}

} // end namespace intrinsic
} // end namespace flang
