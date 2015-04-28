//===----- IntrinsicFunctions.h - enum values for intrinsic functions -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the FunctionKind enum and support functions.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_INTRINSICFUNCTIONS_H__
#define FLANG_AST_INTRINSICFUNCTIONS_H__

#include "flang/Basic/IdentifierTable.h"
#include "flang/Basic/LangOptions.h"
#include "llvm/ADT/StringMap.h"

namespace flang {
namespace intrinsic {

enum FunctionArgumentCountKind {
  ArgumentCount1,
  ArgumentCount2,
  ArgumentCount3,
  ArgumentCount1or2,
  ArgumentCount2orMore
};

/// FunctionKind - This provides a simple uniform namespace for
/// intrinsic functions from all Fortran languages.
enum FunctionKind {
#define INTRINSIC_FUNCTION(NAME, GENERICNAME, NUMARGS, VERSION) NAME,
#include "IntrinsicFunctions.def"
  NUM_FUNCTIONS
};

/// Group - the list of function groups.
enum Group {
  GROUP_NONE,
#define INTRINSIC_GROUP(NAME, FIRST, LAST) GROUP_ ## NAME,
#include "IntrinsicFunctions.def"
  NUM_GROUPS
};

/// \brief Returns the id of the generic function of this overload.
FunctionKind getGenericFunctionKind(FunctionKind Function);

/// \brief Returns the id of the group that this function belongs to.
Group getFunctionGroup(FunctionKind Function);

/// \brief Returns the name of the function.
const char *getFunctionName(FunctionKind Kind);

/// \brief Returns the number of arguments that the function accepts.
FunctionArgumentCountKind getFunctionArgumentCount(FunctionKind Function);

/// Maps the intrinsic function identifiers to function IDs
class FunctionMapping {
  llvm::StringMap<FunctionKind> Mapping;
public:
  FunctionMapping(const LangOptions &Options);

  struct Result {
    FunctionKind Function;
    bool IsInvalid;
  };

  Result Resolve(const IdentifierInfo *IDInfo);
};

}  // end namespace intrinsic
}  // end namespace flang

#endif
