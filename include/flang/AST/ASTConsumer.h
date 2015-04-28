//===--- ASTConsumer.h - Abstract interface for reading ASTs ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ASTConsumer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_AST_ASTCONSUMER_H
#define LLVM_FLANG_AST_ASTCONSUMER_H

namespace flang {
  class ASTContext;
  class FunctionDecl;

/// ASTConsumer - This is an abstract interface that should be implemented by
/// clients that read ASTs.  This abstraction layer allows the client to be
/// independent of the AST producer (e.g. parser vs AST dump file reader, etc).
class ASTConsumer {
public:
  ASTConsumer() { }

  virtual ~ASTConsumer() {}

  /// Initialize - This is called to initialize the consumer, providing the
  /// ASTContext.
  virtual void Initialize(ASTContext &Context) {}

  /// HandleTranslationUnit - This method is called when the ASTs for entire
  /// translation unit have been parsed.
  virtual void HandleTranslationUnit(ASTContext &Ctx) {}

  /// PrintStats - If desired, print any statistics.
  virtual void PrintStats() {}
};

} // end namespace flang.

#endif
