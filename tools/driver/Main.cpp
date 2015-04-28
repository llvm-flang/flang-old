//===-- Main.cpp - LLVM-based Fortran Compiler ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran Compiler.
//
//===----------------------------------------------------------------------===//


#include "flang/Frontend/TextDiagnosticPrinter.h"
#include "flang/Frontend/VerifyDiagnosticConsumer.h"
#include "flang/AST/ASTConsumer.h"
#include "flang/Frontend/ASTConsumers.h"
#include "flang/Parse/Parser.h"
#include "flang/Sema/Sema.h"
#include "flang/CodeGen/ModuleBuilder.h"
#include "flang/CodeGen/BackendUtil.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
//#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Timer.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/ObjCARC.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/SymbolRewriter.h"
#include <memory>

using namespace llvm;
using namespace flang;

//===----------------------------------------------------------------------===//
// Command line options.
//===----------------------------------------------------------------------===//

namespace {

  cl::list<std::string>
  InputFiles(cl::Positional, cl::desc("<input file>"));

  cl::list<std::string>
  IncludeDirs("I", cl::desc("Directory of include files"),
              cl::value_desc("directory"), cl::Prefix);

  cl::opt<bool>
  ReturnComments("C", cl::desc("Do not discard comments"), cl::init(false));

  cl::opt<bool>
  RunVerifier("verify", cl::desc("Run the verifier"), cl::init(false));

  cl::opt<bool>
  SyntaxOnly("fsyntax-only", cl::desc("Do not compile code"), cl::init(false));

  cl::opt<bool>
  PrintAST("ast-print", cl::desc("Prints AST"), cl::init(false));

  cl::opt<bool>
  DumpAST("ast-dump", cl::desc("Dumps AST"), cl::init(false));

  cl::opt<bool>
  EmitLLVM("emit-llvm", cl::desc("Emit llvm"), cl::init(false));

  cl::opt<bool>
  EmitASM("S", cl::desc("Emit assembly"), cl::init(false));

  cl::opt<std::string>
  OutputFile("o", cl::desc("<output file>"), cl::init(""));

  cl::opt<int>
  OptLevel("O", cl::desc("optimization level"), cl::init(0), cl::Prefix);

  cl::opt<bool>
  EmitDebugInfo("g", cl::desc("Emit debugging info"), cl::init(false));

  cl::list<std::string>
  LinkDirectories("L", cl::desc("Additional directories for library files"), cl::Prefix);

  cl::list<std::string>
  LinkLibraries("l", cl::desc("Additional libraries"), cl::Prefix);

  cl::opt<bool>
  CompileOnly("c", cl::desc("compile only, do not link"), cl::init(false));

  cl::opt<bool>
  Interpret("interpret", cl::desc("run the code from the given input"), cl::init(false));

  cl::opt<std::string>
  TargetTriple("triple", cl::desc("target triple"), cl::init(""));

  cl::opt<bool>
  DefaultReal8("fdefault-real-8", cl::desc("set the kind of the default real type to 8"), cl::init(false));

  cl::opt<bool>
  DefaultDouble8("fdefault-double-8", cl::desc("set the kind of the default double type to 8"), cl::init(false));

  cl::opt<bool>
  DefaultInt8("fdefault-integer-8", cl::desc("set the kind of the default integer type to 8"), cl::init(false));

  cl::opt<bool>
  FreeForm("ffree-form", cl::desc("the source files are using free form layout"), cl::init(false));

  cl::opt<bool>
  FixedForm("ffixed-form", cl::desc("the source files are using fixed form layout"), cl::init(false));

} // end anonymous namespace


/*
extern "C" void jit_write_start(void *) { }

extern "C" void jit_write_character(void *,const char *Ptr, size_t Length) {
  for(size_t I = 0; I < Length; ++I)
    llvm::outs() << Ptr[I];
}

extern "C" void jit_write_integer(void *,const void *Ptr, int32_t Size) {
  if(Size != 4) return;
  llvm::outs() << *reinterpret_cast<const int32_t*>(Ptr);
}

extern "C" void jit_write_logical(void *,const void *Ptr, int32_t Size) {
  if(Size != 4) return;
  llvm::outs() << ((*reinterpret_cast<const int32_t*>(Ptr)) != 0? "true" : "false");
}

extern "C" void jit_write_end(void *) {
  llvm::outs() << "\n";
}

extern "C" void jit_assignment_char1(char *LHS, size_t LHSLength,
                                     char *RHS, size_t RHSLength) {
  memmove(LHS, RHS, std::min(LHSLength, RHSLength));
}

extern "C" void jit_init() {}

static int Execute(llvm::Module *Module, const char * const *envp) {
  std::string Error;
  std::unique_ptr<llvm::ExecutionEngine> EE(
    llvm::ExecutionEngine::createJIT(Module, &Error));
  if (!EE) {
    llvm::errs() << "unable to make execution engine: " << Error << "\n";
    return -1;
  }

  llvm::Function *EntryFn = Module->getFunction("main");
  if (!EntryFn) {
    llvm::errs() << "'main' function not found in module.\n";
    return -1;
  }

  if(auto F = Module->getFunction("libflang_write_start")) {
    EE->addGlobalMapping(F, (void*) &jit_write_start);
  }
  if(auto F = Module->getFunction("libflang_write_end")) {
    EE->addGlobalMapping(F, (void*) &jit_write_end);
  }
  if(auto F = Module->getFunction("libflang_write_character")) {
    EE->addGlobalMapping(F, (void*) &jit_write_character);
  }
  if(auto F = Module->getFunction("libflang_write_integer")) {
    EE->addGlobalMapping(F, (void*) &jit_write_integer);
  }
  if(auto F = Module->getFunction("libflang_write_logical")) {
    EE->addGlobalMapping(F, (void*) &jit_write_logical);
  }
  if(auto F = Module->getFunction("libflang_assignment_char1")) {
    EE->addGlobalMapping(F, (void*) &jit_assignment_char1);
  }
  if(auto F = Module->getFunction("libflang_malloc")) {
    EE->addGlobalMapping(F, (void*) &malloc);
  }
  if(auto F = Module->getFunction("libflang_free")) {
    EE->addGlobalMapping(F, (void*) &free);
  }
  if(auto F = Module->getFunction("libflang_sys_init")) {
    EE->addGlobalMapping(F, (void*) &jit_init);
  }

  std::vector<std::string> Args;
  Args.push_back(Module->getModuleIdentifier());

  return EE->runFunctionAsMain(EntryFn, Args, envp);
}
*/

std::string GetOutputName(StringRef Filename,
                          BackendAction Action) {
  llvm::SmallString<256> Path(Filename.begin(), Filename.end());
  switch(Action) {
  case Backend_EmitObj:
    llvm::sys::path::replace_extension(Path, ".o");
    break;
  case Backend_EmitAssembly:
    llvm::sys::path::replace_extension(Path, ".s");
    break;
  case Backend_EmitBC:
    llvm::sys::path::replace_extension(Path, ".bc");
    break;
  case Backend_EmitLL:
    llvm::sys::path::replace_extension(Path, ".ll");
    break;
  }
  return std::string(Path.begin(), Path.size());
}

static bool EmitFile(llvm::raw_ostream &Out,
                     llvm::Module *Module,
                     llvm::TargetMachine* TM,
                     BackendAction Action) {
  //write instructions to file
  if(Action == Backend_EmitObj || Action == Backend_EmitAssembly){
    llvm::Module &Mod = *Module;
    llvm::TargetMachine &Target = *TM;
    llvm::TargetMachine::CodeGenFileType FileType =
      Action == Backend_EmitObj ? llvm::TargetMachine::CGFT_ObjectFile :
                                  llvm::TargetMachine::CGFT_AssemblyFile;

    llvm::legacy::PassManager PM;

    //Target.setAsmVerbosityDefault(true);
    //Target.setMCRelaxAll(true);
    llvm::formatted_raw_ostream FOS(Out);

    //FIXME : add the backend passes 
    // Ask the target to add backend passes as necessary.
    //if (Target.addPassesToEmitFile(PM, FOS, FileType, true)) {
    //  return true;
    //}

    PM.run(Mod);
  }
  else if(Action == Backend_EmitBC ){
    llvm::WriteBitcodeToFile(Module, Out);
  } else if(Action == Backend_EmitLL ) {
    Module->print(Out, nullptr);
  }
  return false;
}

static bool EmitOutputFile(const std::string &Input,
                           llvm::Module *Module,
                           llvm::TargetMachine* TM,
                           BackendAction Action) {
  std::error_code err;
  llvm::raw_fd_ostream Out(Input.c_str(), err, llvm::sys::fs::F_None);
  if (err){
    llvm::errs() << "Could not open output file '" << Input << "': "
                 << err.message() <<"\n";
    return true;
  }
  return EmitFile(Out, Module, TM, Action);
}

static bool LinkFiles(ArrayRef<std::string> OutputFiles) {
  const char *Driver = "gcc";
  std::string Cmd;
  llvm::raw_string_ostream OS(Cmd);
  OS << Driver;
  for(const std::string &I : OutputFiles)
    OS << " " << I;
  for(const std::string &I : LinkDirectories)
    OS << " -L " << I;
  OS << " -l libflang";
  for(const std::string &I : LinkLibraries)
    OS << " -l " << I;
  // Link with the math library.
  OS << " -l m";
  if(OutputFile.size())
    OS << " -o " << OutputFile;
  Cmd = OS.str();
  return system(Cmd.c_str());
}

static bool ParseFile(const std::string &Filename,
                      const std::vector<std::string> &IncludeDirs,
                      SmallVectorImpl<std::string> &OutputFiles) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> MBOrErr =
      MemoryBuffer::getFileOrSTDIN(Filename);
  if (std::error_code EC = MBOrErr.getError()) {
    llvm::errs() << "Could not open input file '" << Filename << "': " 
                 << EC.message() <<"\n";
    return true;
  }
  std::unique_ptr<llvm::MemoryBuffer> MB = std::move(MBOrErr.get());

  // Record the location of the include directory so that the lexer can find it
  // later.
  SourceMgr SrcMgr;
  SrcMgr.setIncludeDirs(IncludeDirs);

  // Tell SrcMgr about this buffer, which is what Parser will pick up.
  SrcMgr.AddNewSourceBuffer(std::move(MB), llvm::SMLoc());

  LangOptions Opts;
  Opts.DefaultReal8 = DefaultReal8;
  Opts.DefaultDouble8 = DefaultDouble8;
  Opts.DefaultInt8 = DefaultInt8;
  Opts.ReturnComments = ReturnComments;
  if(!FreeForm && !FixedForm) {
    llvm::StringRef Ext = llvm::sys::path::extension(Filename);
    if(Ext.equals_lower(".f")) {
      Opts.FixedForm = 1;
      Opts.FreeForm = 0;
    }
  } else if(FixedForm) {
    Opts.FixedForm = 1;
    Opts.FreeForm = 0;
  }

  TextDiagnosticPrinter TDP(SrcMgr);
  DiagnosticsEngine Diag(new DiagnosticIDs,&SrcMgr, &TDP, false);
  // Chain in -verify checker, if requested.
  if(RunVerifier)
    Diag.setClient(new VerifyDiagnosticConsumer(Diag));

  ASTContext Context(SrcMgr, Opts);
  Sema SA(Context, Diag);
  Parser P(SrcMgr, Opts, Diag, SA);
  Diag.getClient()->BeginSourceFile(Opts, &P.getLexer());
  P.ParseProgramUnits();
  Diag.getClient()->EndSourceFile();

  // Dump
  if(PrintAST || DumpAST) {
    auto Dumper = CreateASTDumper("");
    Dumper->HandleTranslationUnit(Context);
    delete Dumper;
  }

  // Emit
  if(!SyntaxOnly && !Diag.hadErrors()) {    
    flang::TargetOptions TargetOptions;
    TargetOptions.Triple = TargetTriple.empty()? llvm::sys::getDefaultTargetTriple() :
                                                 TargetTriple;
    TargetOptions.CPU = llvm::sys::getHostCPUName();

    auto CG = CreateLLVMCodeGen(Diag, Filename == ""? std::string("module") : Filename,
                                CodeGenOptions(), TargetOptions, llvm::getGlobalContext());
    CG->Initialize(Context);
    CG->HandleTranslationUnit(Context);

    BackendAction BA = Backend_EmitObj;
    if(EmitASM)   BA = Backend_EmitAssembly;
    if(EmitLLVM)  BA = Backend_EmitLL;

    const llvm::Target *TheTarget = 0;
    std::string Err;
    TheTarget = llvm::TargetRegistry::lookupTarget(TargetOptions.Triple, Err);

    CodeGenOpt::Level TMOptLevel = CodeGenOpt::Default;
    switch(OptLevel) {
    case 0:  TMOptLevel = CodeGenOpt::None; break;
    case 3:  TMOptLevel = CodeGenOpt::Aggressive; break;
    }

    llvm::TargetOptions Options;

    auto TM = TheTarget->createTargetMachine(TargetOptions.Triple, TargetOptions.CPU, "", Options,
                                             Reloc::Default, CodeModel::Default,
                                             TMOptLevel);

    if(!(EmitLLVM && OptLevel == 0)) {
      auto TheModule = CG->GetModule();
      auto PM = new llvm::legacy::PassManager();
      //llvm::legacy::FunctionPassManager *FPM = new llvm::legacy::FunctionPassManager(TheModule);
      //FPM->add(new DataLayoutPass());
      //PM->add(new llvm::DataLayoutPass());
      //TM->addAnalysisPasses(*PM);
      PM->add(createPromoteMemoryToRegisterPass());

      PassManagerBuilder PMBuilder;
      PMBuilder.OptLevel = OptLevel;
      PMBuilder.SizeLevel = 0;
      PMBuilder.LoopVectorize = true;
      PMBuilder.SLPVectorize = true;
      unsigned Threshold = 225;
      if (OptLevel > 2)
        Threshold = 275;
      PMBuilder.Inliner = createFunctionInliningPass(Threshold);


      PMBuilder.populateModulePassManager(*PM);
      //llvm::legacy::PassManager *MPM = new llvm::legacy::PassManager();
      //PMBuilder.populateModulePassManager(*MPM);

      PM->run(*TheModule);
      //MPM->run(*TheModule);
      delete PM;
      //delete MPM;
    }

    if(Interpret) {
      //const char *Env[] = { "", nullptr };
      //Execute(CG->ReleaseModule(), Env);
    } else if(OutputFile == "-"){
      // FIXME: outputting to stdout is broken 
      //EmitFile(llvm::outs(), CG->GetModule(), TM, BA);
      OutputFiles.push_back(GetOutputName("stdout",BA));
      EmitOutputFile(OutputFiles.back(), CG->GetModule(), TM, BA);
    }else {
      OutputFiles.push_back(GetOutputName(Filename, BA));
      EmitOutputFile(OutputFiles.back(), CG->GetModule(), TM, BA);
    }
    delete CG;
  }

  return Diag.hadErrors();
}

int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "LLVM Fortran compiler");

  bool CanonicalPrefixes = true;
  for (int i = 1; i < argc; ++i)
    if (llvm::StringRef(argv[i]) == "-no-canonical-prefixes") {
      CanonicalPrefixes = false;
      break;
    }

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Parse the input file.
  bool HadErrors = false;
  SmallVector <std::string, 32> OutputFiles;
  OutputFiles.reserve(1);

  if(InputFiles.empty())
    InputFiles.push_back("-");
  for(auto I : InputFiles) {
    llvm::StringRef Ext = llvm::sys::path::extension(I);
    if(Ext.equals_lower(".o") || Ext.equals_lower(".obj") ||
       Ext.equals_lower(".a") || Ext.equals_lower(".lib"))
      OutputFiles.push_back(I);
    else if(ParseFile(I, IncludeDirs, OutputFiles))
      HadErrors = true;
  }
  if(OutputFiles.size() && !HadErrors && !CompileOnly && !EmitLLVM && !EmitASM)
    LinkFiles(OutputFiles);

  // If any timers were active but haven't been destroyed yet, print their
  // results now. This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());

  llvm::llvm_shutdown();
  return HadErrors;
}
