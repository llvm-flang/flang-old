//===--- Diagnostic.h - Fortran Language Diagnostic Handling ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the Diagnostic-related interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_DIAGNOSTIC_H__
#define FLANG_DIAGNOSTIC_H__

#include "flang/Basic/DiagnosticIDs.h"
#include "flang/Basic/SourceLocation.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/DenseMap.h"
#include <list>

namespace llvm {
  class SourceMgr;
  class Twine;
  class StringRef;
} // end namespace llvm

namespace flang {

class DiagnosticClient;
class DiagnosticBuilder;
class Diagnostic;
class DiagnosticErrorTrap;
class LangOptions;
class IdentifierInfo;
class Lexer;

typedef llvm::SMFixIt FixItHint;

/// DiagnosticEngine - This concrete class is used by the front-end to report problems
/// and issues. It manages the diagnostics and passes them off to the
/// DiagnosticClient for reporting to the user.
class DiagnosticsEngine : public llvm::RefCountedBase<DiagnosticsEngine> {
public:
  /// Level - The level of the diagnostic, after it has been through mapping.
  enum Level {
    Ignored = 0,
    Note    = 1,
    Warning = 2,
    Error   = 3,
    Fatal   = 4
  };

  enum ArgumentKind {
    ak_std_string,      ///< std::string
    ak_c_string,        ///< const char *
    ak_sint,            ///< int
    ak_uint,            ///< unsigned
    ak_identifierinfo,  ///< IdentifierInfo
    ak_qualtype,        ///< QualType
    ak_declarationname, ///< DeclarationName
    ak_nameddecl,       ///< NamedDecl *
    ak_nestednamespec,  ///< NestedNameSpecifier *
    ak_declcontext,     ///< DeclContext *
    ak_qualtype_pair    ///< pair<QualType, QualType>
  };

  /// \brief Represents on argument value, which is a union discriminated
  /// by ArgumentKind, with a value.
  typedef std::pair<ArgumentKind, intptr_t> ArgumentValue;

private:
  DiagnosticClient *Client;
  bool OwnsDiagClient;
  llvm::SourceMgr *SrcMgr;
  unsigned ErrorLimit;           // Cap of # errors emitted, 0 -> no limit.
  llvm::IntrusiveRefCntPtr<DiagnosticIDs> Diags;

  /// \brief Mapping information for diagnostics.
  ///
  /// Mapping info is packed into four bits per diagnostic.  The low three
  /// bits are the mapping (an instance of diag::Mapping), or zero if unset.
  /// The high bit is set when the mapping was established as a user mapping.
  /// If the high bit is clear, then the low bits are set to the default
  /// value, and should be mapped with -pedantic, -Werror, etc.
  ///
  /// A new DiagState is created and kept around when diagnostic pragmas modify
  /// the state so that we know what is the diagnostic state at any given
  /// source location.
  class DiagState {
    llvm::DenseMap<unsigned, DiagnosticMappingInfo> DiagMap;

  public:
    typedef llvm::DenseMap<unsigned, DiagnosticMappingInfo>::iterator
      iterator;
    typedef llvm::DenseMap<unsigned, DiagnosticMappingInfo>::const_iterator
      const_iterator;

    void setMappingInfo(diag::kind Diag, DiagnosticMappingInfo Info) {
      DiagMap[Diag] = Info;
    }

    DiagnosticMappingInfo &getOrAddMappingInfo(diag::kind Diag);

    const_iterator begin() const { return DiagMap.begin(); }
    const_iterator end() const { return DiagMap.end(); }
  };

  /// \brief Keeps and automatically disposes all DiagStates that we create.
  std::list<DiagState> DiagStates;

  /// \brief Represents a point in source where the diagnostic state was
  /// modified because of a pragma.
  ///
  /// 'Loc' can be null if the point represents the diagnostic state
  /// modifications done through the command-line.
  struct DiagStatePoint {
    DiagState *State;
    SourceLocation Loc;
    DiagStatePoint(DiagState *State, SourceLocation Loc)
      : State(State), Loc(Loc) { }

    bool operator<(const DiagStatePoint &RHS) const {
      // If Loc is invalid it means it came from <command-line>, in which case
      // we regard it as coming before any valid source location.
      if (!RHS.Loc.isValid())
        return false;
      if (!Loc.isValid())
        return true;
      return Loc.getPointer() < RHS.Loc.getPointer();
    }
  };

  /// \brief A sorted vector of all DiagStatePoints representing changes in
  /// diagnostic state due to diagnostic pragmas.
  ///
  /// The vector is always sorted according to the SourceLocation of the
  /// DiagStatePoint.
  typedef std::vector<DiagStatePoint> DiagStatePointsTy;
  mutable DiagStatePointsTy DiagStatePoints;

  /// \brief Keeps the DiagState that was active during each diagnostic 'push'
  /// so we can get back at it when we 'pop'.
  std::vector<DiagState *> DiagStateOnPushStack;

  DiagState *GetCurDiagState() const {
    assert(!DiagStatePoints.empty());
    return DiagStatePoints.back().State;
  }

  void PushDiagStatePoint(DiagState *State, SourceLocation L) {
    SourceLocation Loc(L);
    // Make sure that DiagStatePoints is always sorted according to Loc.
    assert(Loc.isValid() && "Adding invalid loc point");
    assert(!DiagStatePoints.empty() &&
           (DiagStatePoints.back().Loc.isValid() ||
            DiagStatePoints.back().Loc.getPointer() < Loc.getPointer()) &&
           "Previous point loc comes after or is the same as new one");
    DiagStatePoints.push_back(DiagStatePoint(State, Loc));
  }

  /// \brief Finds the DiagStatePoint that contains the diagnostic state of
  /// the given source location.
  DiagStatePointsTy::iterator GetDiagStatePointForLoc(SourceLocation Loc) const;

  /// \brief Sticky flag set to \c true when an error is emitted.
  bool ErrorOccurred;

  /// \brief Sticky flag set to \c true when an "uncompilable error" occurs.
  /// I.e. an error that was not upgraded from a warning by -Werror.
  bool UncompilableErrorOccurred;

  /// \brief Sticky flag set to \c true when a fatal error is emitted.
  bool FatalErrorOccurred;

  /// \brief Indicates that an unrecoverable error has occurred.
  bool UnrecoverableErrorOccurred;

  /// \brief Counts for DiagnosticErrorTrap to check whether an error occurred
  /// during a parsing section, e.g. during parsing a function.
  unsigned TrapNumErrorsOccurred;
  unsigned TrapNumUnrecoverableErrorsOccurred;

  /// \brief The level of the last diagnostic emitted.
  ///
  /// This is used to emit continuation diagnostics with the same level as the
  /// diagnostic that they follow.
  DiagnosticIDs::Level LastDiagLevel;

  unsigned NumWarnings;         ///< Number of warnings reported
  unsigned NumErrors;           ///< Number of errors reported
  unsigned NumErrorsSuppressed; ///< Number of errors suppressed

  /// \brief ID of the "delayed" diagnostic, which is a (typically
  /// fatal) diagnostic that had to be delayed because it was found
  /// while emitting another diagnostic.
  unsigned DelayedDiagID;

  /// \brief First string argument for the delayed diagnostic.
  std::string DelayedDiagArg1;

  /// \brief Second string argument for the delayed diagnostic.
  std::string DelayedDiagArg2;
public:
  DiagnosticsEngine(const llvm::IntrusiveRefCntPtr<DiagnosticIDs> &D,
                    llvm::SourceMgr *SM, DiagnosticClient *DC,
                    bool ShouldOwnClient = true)
    : Client(DC), OwnsDiagClient(ShouldOwnClient), SrcMgr(SM), Diags(D)
  { Reset(); }

  const llvm::IntrusiveRefCntPtr<DiagnosticIDs> &getDiagnosticIDs() const {
    return Diags;
  }

  DiagnosticClient *getClient() { return Client; }
  const DiagnosticClient *getClient() const { return Client; }

  bool hadErrors();
  bool hadWarnings();

  /// \brief Return the current diagnostic client along with ownership of that
  /// client.
  DiagnosticClient *takeClient() {
    OwnsDiagClient = false;
    return Client;
  }

  /// \brief Return true if the current diagnostic client is owned by this class.
  bool ownsClient() {
    return OwnsDiagClient;
  }

  bool hasSourceManager() const { return SrcMgr != 0; }
  llvm::SourceMgr &getSourceManager() const {
    assert(SrcMgr && "SourceManager not set!");
    return *SrcMgr;
  }
  void setSourceManager(llvm::SourceMgr *SM) { SrcMgr = SM; }

  /// \brief Set the diagnostic client associated with this diagnostic object.
  ///
  /// \param ShouldOwnClient true if the diagnostic object should take
  /// ownership of \c client.
  void setClient(DiagnosticClient *client, bool ShouldOwnClient = true) {
    Client = client;
    OwnsDiagClient = ShouldOwnClient;
  }

  /// \brief Specify a limit for the number of errors we should
  /// emit before giving up.
  ///
  /// Zero disables the limit.
  void setErrorLimit(unsigned Limit) { ErrorLimit = Limit; }

  /// \brief This allows the client to specify that certain warnings are
  /// ignored.
  ///
  /// Notes can never be mapped, errors can only be mapped to fatal, and
  /// WARNINGs and EXTENSIONs can be mapped arbitrarily.
  ///
  /// \param Loc The source location that this change of diagnostic state should
  /// take affect. It can be null if we are setting the latest state.
  void setDiagnosticMapping(diag::kind Diag, diag::Mapping Map,
                            SourceLocation Loc);

  /// \brief Reset the state of the diagnostic object to its initial
  /// configuration.
  void Reset();

  /// \brief Issue the message to the client.
  ///
  /// This actually returns an instance of DiagnosticBuilder which emits the
  /// diagnostics (through @c ProcessDiag) when it is destroyed.
  ///
  /// \param DiagID A member of the @c diag::kind enum.
  /// \param Loc Represents the source location associated with the diagnostic,
  /// which can be an invalid location if no position information is available.
  inline DiagnosticBuilder Report(SourceLocation Loc, unsigned DiagID);
  inline DiagnosticBuilder Report(unsigned DiagID);

  /// ReportError - Emit an error at the location \arg L, with the message \arg
  /// Msg.
  ///
  /// \return The return value is always true, as an idiomatic convenience to
  /// clients.
  bool ReportError(SourceLocation L, const llvm::Twine &Msg);

  /// ReportWarning - Emit a warning at the location \arg L, with the message
  /// \arg Msg.
  ///
  /// \return The return value is always true, as an idiomatic convenience to
  /// clients.
  bool ReportWarning(SourceLocation L, const llvm::Twine &Msg);

  /// ReportNote - Emit a note at the location \arg L, with the message
  /// \arg Msg.
  ///
  /// \return The return value is always true, as an idiomatic convenience to
  /// clients
  bool ReportNote(SourceLocation L, const llvm::Twine &Msg);

  bool hasErrorOccurred() const {
    return NumErrors!=0;
  }

  /// \brief Clear out the current diagnostic.
  void Clear() { CurDiagID = ~0U; }
private:

  /// \brief Report the delayed diagnostic.
  void ReportDelayed();

  // This is private state used by DiagnosticBuilder.  We put it here instead of
  // in DiagnosticBuilder in order to keep DiagnosticBuilder a small lightweight
  // object.  This implementation choice means that we can only have one
  // diagnostic "in flight" at a time, but this seems to be a reasonable
  // tradeoff to keep these objects small.  Assertions verify that only one
  // diagnostic is in flight at a time.
  friend class DiagnosticIDs;
  friend class DiagnosticBuilder;
  friend class Diagnostic;
  friend class DiagnosticErrorTrap;

  /// \brief The location of the current diagnostic that is in flight.
  SourceLocation CurDiagLoc;

  /// \brief The ID of the current diagnostic that is in flight.
  ///
  /// This is set to ~0U when there is no diagnostic in flight.
  unsigned CurDiagID;

  enum {
    /// \brief The maximum number of arguments we can hold.
    ///
    /// We currently only support up to 10 arguments (%0-%9).  A single
    /// diagnostic with more than that almost certainly has to be simplified
    /// anyway.
    MaxArguments = 10,

    /// \brief The maximum number of ranges we can hold.
    MaxRanges = 10,

    /// \brief The maximum number of ranges we can hold.
    MaxFixItHints = 10
  };

  /// \brief The number of entries in Arguments.
  signed char NumDiagArgs;
  /// \brief The number of ranges in the DiagRanges array.
  unsigned char NumDiagRanges;
  /// \brief The number of hints in the DiagFixItHints array.
  unsigned char NumDiagFixItHints;

  /// \brief Specifies whether an argument is in DiagArgumentsStr or
  /// in DiagArguments.
  ///
  /// This is an array of ArgumentKind::ArgumentKind enum values, one for each
  /// argument.
  unsigned char DiagArgumentsKind[MaxArguments];

  /// \brief Holds the values of each string argument for the current
  /// diagnostic.
  ///
  /// This is only used when the corresponding ArgumentKind is ak_std_string.
  std::string DiagArgumentsStr[MaxArguments];

  /// \brief The values for the various substitution positions.
  ///
  /// This is used when the argument is not an std::string.  The specific
  /// value is mangled into an intptr_t and the interpretation depends on
  /// exactly what sort of argument kind it is.
  intptr_t DiagArgumentsVal[MaxArguments];

  /// \brief The list of ranges added to this diagnostic.
  SourceRange DiagRanges[MaxRanges];

  SmallVector<FixItHint, 10> DiagFixItHints;

  DiagnosticMappingInfo makeMappingInfo(diag::Mapping Map, SourceLocation L) {
    bool isPragma = L.isValid();
    DiagnosticMappingInfo MappingInfo = DiagnosticMappingInfo::Make(
      Map, /*IsUser=*/true, isPragma);

    // If this is a pragma mapping, then set the diagnostic mapping flags so
    // that we override command line options.
    if (isPragma) {
      MappingInfo.setNoWarningAsError(true);
      MappingInfo.setNoErrorAsFatal(true);
    }

    return MappingInfo;
  }

  /// \brief Used to report a diagnostic that is finally fully formed.
  ///
  /// \returns true if the diagnostic was emitted, false if it was suppressed.
  bool ProcessDiag() {
    return Diags->ProcessDiag(*this);
  }
protected:
  /// \brief Emit the current diagnostic and clear the diagnostic state.
  ///
  /// \param Force Emit the diagnostic regardless of suppression settings.
  bool EmitCurrentDiagnostic(bool Force = false);

  unsigned getCurrentDiagID() const { return CurDiagID; }

  SourceLocation getCurrentDiagLoc() const { return CurDiagLoc; }
};

/// \brief RAII class that determines when any errors have occurred between the
/// time the instance was created and the time it was queried.
class DiagnosticErrorTrap {
  DiagnosticsEngine &Diag;
  unsigned NumErrors;
  unsigned NumUnrecoverableErrors;
public:
  explicit DiagnosticErrorTrap(DiagnosticsEngine &Diag)
    : Diag(Diag) { reset(); }

  /// \brief Determine whether any errors have occurred since this
  /// object instance was created.
  bool hasErrorOccurred() const {
    return Diag.TrapNumErrorsOccurred > NumErrors;
  }

  /// \brief Determine whether any unrecoverable errors have occurred since this
  /// object instance was created.
  bool hasUnrecoverableErrorOccurred() const {
    return Diag.TrapNumUnrecoverableErrorsOccurred > NumUnrecoverableErrors;
  }

  // Set to initial state of "no errors occurred".
  void reset() {
    NumErrors = Diag.TrapNumErrorsOccurred;
    NumUnrecoverableErrors = Diag.TrapNumUnrecoverableErrorsOccurred;
  }
};

//===----------------------------------------------------------------------===//
// DiagnosticBuilder
//===----------------------------------------------------------------------===//

/// \brief A little helper class used to produce diagnostics.
///
/// This is constructed by the DiagnosticsEngine::Report method, and
/// allows insertion of extra information (arguments and source ranges) into
/// the currently "in flight" diagnostic.  When the temporary for the builder
/// is destroyed, the diagnostic is issued.
///
/// Note that many of these will be created as temporary objects (many call
/// sites), so we want them to be small and we never want their address taken.
/// This ensures that compilers with somewhat reasonable optimizers will promote
/// the common fields to registers, eliminating increments of the NumArgs field,
/// for example.
class DiagnosticBuilder {
  mutable DiagnosticsEngine *DiagObj;
  mutable unsigned NumArgs, NumRanges, NumFixits;

  /// \brief Status variable indicating if this diagnostic is still active.
  ///
  // NOTE: This field is redundant with DiagObj (IsActive iff (DiagObj == 0)),
  // but LLVM is not currently smart enough to eliminate the null check that
  // Emit() would end up with if we used that as our status variable.
  mutable bool IsActive;

  /// \brief Flag indicating that this diagnostic is being emitted via a
  /// call to ForceEmit.
  mutable bool IsForceEmit;

  void operator=(const DiagnosticBuilder &) = delete;
  friend class DiagnosticsEngine;

  DiagnosticBuilder()
    : DiagObj(0), NumArgs(0), NumRanges(0), NumFixits(0), IsActive(false),
      IsForceEmit(false) { }

  explicit DiagnosticBuilder(DiagnosticsEngine *diagObj)
    : DiagObj(diagObj), NumArgs(0), NumRanges(0), NumFixits(0), IsActive(true),
      IsForceEmit(false) {
    assert(diagObj && "DiagnosticBuilder requires a valid DiagnosticsEngine!");
  }

  friend class PartialDiagnostic;

protected:
  void FlushCounts() {
    DiagObj->NumDiagArgs = NumArgs;
    DiagObj->NumDiagRanges = NumRanges;
    DiagObj->NumDiagFixItHints = NumFixits;
  }

  /// \brief Clear out the current diagnostic.
  void Clear() const {
    DiagObj = 0;
    IsActive = false;
    IsForceEmit = false;
  }

  /// \brief Determine whether this diagnostic is still active.
  bool isActive() const { return IsActive; }

  /// \brief Force the diagnostic builder to emit the diagnostic now.
  ///
  /// Once this function has been called, the DiagnosticBuilder object
  /// should not be used again before it is destroyed.
  ///
  /// \returns true if a diagnostic was emitted, false if the
  /// diagnostic was suppressed.
  bool Emit() {
    // If this diagnostic is inactive, then its soul was stolen by the copy ctor
    // (or by a subclass, as in SemaDiagnosticBuilder).
    if (!isActive()) return false;

    // When emitting diagnostics, we set the final argument count into
    // the DiagnosticsEngine object.
    FlushCounts();

    // Process the diagnostic.
    bool Result = DiagObj->EmitCurrentDiagnostic(IsForceEmit);

    // This diagnostic is dead.
    Clear();

    return Result;
  }

public:
  /// Copy constructor.  When copied, this "takes" the diagnostic info from the
  /// input and neuters it.
  DiagnosticBuilder(const DiagnosticBuilder &D) {
    DiagObj = D.DiagObj;
    IsActive = D.IsActive;
    IsForceEmit = D.IsForceEmit;
    D.Clear();
    NumArgs = D.NumArgs;
    NumRanges = D.NumRanges;
    NumFixits = D.NumFixits;
  }

  /// \brief Retrieve an empty diagnostic builder.
  static DiagnosticBuilder getEmpty() {
    return DiagnosticBuilder();
  }

  /// \brief Emits the diagnostic.
  ~DiagnosticBuilder() {
    Emit();
  }

  /// \brief Forces the diagnostic to be emitted.
  const DiagnosticBuilder &setForceEmit() const {
    IsForceEmit = true;
    return *this;
  }

  /// \brief Conversion of DiagnosticBuilder to bool always returns \c true.
  ///
  /// This allows is to be used in boolean error contexts (where \c true is
  /// used to indicate that an error has occurred), like:
  /// \code
  /// return Diag(...);
  /// \endcode
  operator bool() const { return true; }

  void AddString(llvm::StringRef S) const {
    assert(isActive() && "Clients must not add to cleared diagnostic!");
    assert(NumArgs < DiagnosticsEngine::MaxArguments &&
           "Too many arguments to diagnostic!");
    DiagObj->DiagArgumentsKind[NumArgs] = DiagnosticsEngine::ak_std_string;
    DiagObj->DiagArgumentsStr[NumArgs++] = S;
  }

  void AddTaggedVal(intptr_t V, DiagnosticsEngine::ArgumentKind Kind) const {
    assert(isActive() && "Clients must not add to cleared diagnostic!");
    assert(NumArgs < DiagnosticsEngine::MaxArguments &&
           "Too many arguments to diagnostic!");
    DiagObj->DiagArgumentsKind[NumArgs] = Kind;
    DiagObj->DiagArgumentsVal[NumArgs++] = V;
  }

  void AddSourceRange(const SourceRange &R) const {
    assert(isActive() && "Clients must not add to cleared diagnostic!");
    assert(NumRanges < DiagnosticsEngine::MaxRanges &&
           "Too many arguments to diagnostic!");
    DiagObj->DiagRanges[NumRanges++] = R;
  }

  void AddFixItHint(const FixItHint &Hint) const {
    DiagObj->DiagFixItHints.push_back(Hint);
  }

  bool hasMaxRanges() const {
    return NumRanges == DiagnosticsEngine::MaxRanges;
  }
};

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           llvm::StringRef S) {
  DB.AddString(S);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const char *Str) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(Str),
                  DiagnosticsEngine::ak_c_string);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB, int I) {
  DB.AddTaggedVal(I, DiagnosticsEngine::ak_sint);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,bool I) {
  DB.AddTaggedVal(I, DiagnosticsEngine::ak_sint);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           unsigned I) {
  DB.AddTaggedVal(I, DiagnosticsEngine::ak_uint);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const IdentifierInfo *II) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(II),
                  DiagnosticsEngine::ak_identifierinfo);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const SourceRange &R) {
  DB.AddSourceRange(R);
  return DB;
}

inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           const FixItHint &Hint) {
  DB.AddFixItHint(Hint);
  return DB;
}

inline DiagnosticBuilder DiagnosticsEngine::Report(SourceLocation Loc,
                                            unsigned DiagID){
  assert(CurDiagID == ~0U && "Multiple diagnostics in flight at once!");
  CurDiagLoc = Loc;
  CurDiagID = DiagID;
  return DiagnosticBuilder(this);
}
inline DiagnosticBuilder DiagnosticsEngine::Report(unsigned DiagID) {
  return Report(SourceLocation(), DiagID);
}

//===----------------------------------------------------------------------===//
// Diagnostic
//===----------------------------------------------------------------------===//

/// A little helper class (which is basically a smart pointer that forwards
/// info from DiagnosticsEngine) that allows clients to enquire about the
/// currently in-flight diagnostic.
class Diagnostic {
  const DiagnosticsEngine *DiagObj;
  llvm::StringRef StoredDiagMessage;
public:
  explicit Diagnostic(const DiagnosticsEngine *DO) : DiagObj(DO) {}
  Diagnostic(const DiagnosticsEngine *DO, llvm::StringRef storedDiagMessage)
    : DiagObj(DO), StoredDiagMessage(storedDiagMessage) {}

  const DiagnosticsEngine *getDiags() const { return DiagObj; }
  unsigned getID() const { return DiagObj->CurDiagID; }
  const SourceLocation &getLocation() const { return DiagObj->CurDiagLoc; }
  bool hasSourceManager() const { return DiagObj->hasSourceManager(); }
  llvm::SourceMgr &getSourceManager() const { return DiagObj->getSourceManager();}

  unsigned getNumArgs() const { return DiagObj->NumDiagArgs; }

  /// \brief Return the kind of the specified index.
  ///
  /// Based on the kind of argument, the accessors below can be used to get
  /// the value.
  ///
  /// \pre Idx < getNumArgs()
  DiagnosticsEngine::ArgumentKind getArgKind(unsigned Idx) const {
    assert(Idx < getNumArgs() && "Argument index out of range!");
    return (DiagnosticsEngine::ArgumentKind)DiagObj->DiagArgumentsKind[Idx];
  }

  /// \brief Return the provided argument string specified by \p Idx.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_std_string
  const std::string &getArgStdStr(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_std_string &&
           "invalid argument accessor!");
    return DiagObj->DiagArgumentsStr[Idx];
  }

  /// \brief Return the specified C string argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_c_string
  const char *getArgCStr(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_c_string &&
           "invalid argument accessor!");
    return reinterpret_cast<const char*>(DiagObj->DiagArgumentsVal[Idx]);
  }

  /// \brief Return the specified signed integer argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_sint
  int getArgSInt(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_sint &&
           "invalid argument accessor!");
    return (int)DiagObj->DiagArgumentsVal[Idx];
  }

  /// \brief Return the specified unsigned integer argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_uint
  unsigned getArgUInt(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_uint &&
           "invalid argument accessor!");
    return (unsigned)DiagObj->DiagArgumentsVal[Idx];
  }

  /// \brief Return the specified IdentifierInfo argument.
  /// \pre getArgKind(Idx) == DiagnosticsEngine::ak_identifierinfo
  const IdentifierInfo *getArgIdentifier(unsigned Idx) const {
    assert(getArgKind(Idx) == DiagnosticsEngine::ak_identifierinfo &&
           "invalid argument accessor!");
    return reinterpret_cast<IdentifierInfo*>(DiagObj->DiagArgumentsVal[Idx]);
  }

  /// \brief Return the specified non-string argument in an opaque form.
  /// \pre getArgKind(Idx) != DiagnosticsEngine::ak_std_string
  intptr_t getRawArg(unsigned Idx) const {
    assert(getArgKind(Idx) != DiagnosticsEngine::ak_std_string &&
           "invalid argument accessor!");
    return DiagObj->DiagArgumentsVal[Idx];
  }

  /// \brief Return the number of source ranges associated with this diagnostic.
  unsigned getNumRanges() const {
    return DiagObj->NumDiagRanges;
  }

  /// \pre Idx < getNumRanges()
  const SourceRange getRange(unsigned Idx) const {
    assert(Idx < DiagObj->NumDiagRanges && "Invalid diagnostic range index!");
    return DiagObj->DiagRanges[Idx];
  }

  /// \brief Return an array reference for this diagnostic's ranges.
  llvm::ArrayRef<SourceRange> getRanges() const {
    return llvm::makeArrayRef(DiagObj->DiagRanges, DiagObj->NumDiagRanges);
  }

  unsigned getNumFixItHints() const {
    return DiagObj->NumDiagFixItHints;
  }

  const ArrayRef<FixItHint> getFixItHint(unsigned Idx) const {
    return DiagObj->DiagFixItHints[Idx];
  }

  const ArrayRef<FixItHint> getFixItHints() const {
    return DiagObj->DiagFixItHints;
  }

  /// \brief Format this diagnostic into a string, substituting the
  /// formal arguments into the %0 slots.
  ///
  /// The result is appended onto the \p OutStr array.
  void FormatDiagnostic(llvm::SmallVectorImpl<char> &OutStr) const;

  /// \brief Format the given format-string into the output buffer using the
  /// arguments stored in this diagnostic.
  void FormatDiagnostic(const char *DiagStr, const char *DiagEnd,
                        llvm::SmallVectorImpl<char> &OutStr) const;
};

/// DiagnosticClient - This is an abstract interface implemented by clients of
/// the front-end, which formats and prints fully processed diagnostics.
class DiagnosticClient {
protected:
  unsigned NumWarnings;       // Number of warnings reported
  unsigned NumErrors;         // Number of errors reported
public:
  DiagnosticClient() : NumWarnings(0), NumErrors(0) { }

  unsigned getNumErrors() const { return NumErrors; }
  unsigned getNumWarnings() const { return NumWarnings; }

  virtual ~DiagnosticClient();

  /// BeginSourceFile - Callback to inform the diagnostic client that processing
  /// of a source file is beginning.
  ///
  /// Note that diagnostics may be emitted outside the processing of a source
  /// file, for example during the parsing of command line options. However,
  /// diagnostics with source range information are required to be emitted only
  /// in between BeginSourceFile() and EndSourceFile().
  ///
  /// \arg LO - The language options for the source file being processed.
  virtual void BeginSourceFile(const LangOptions &, const Lexer *PP) {}

  /// EndSourceFile - Callback to inform the diagnostic client that processing
  /// of a source file has ended. The diagnostic client should assume that any
  /// objects made available via \see BeginSourceFile() are inaccessible.
  virtual void EndSourceFile() {}

  /// IncludeInDiagnosticCounts - This method (whose default implementation
  /// returns true) indicates whether the diagnostics handled by this
  /// DiagnosticClient should be included in the number of diagnostics reported
  /// by Diagnostic.
  virtual bool IncludeInDiagnosticCounts() const { return true; }

  /// HandleDiagnostic - Handle this diagnostic, reporting it to the user or
  /// capturing it to a log as needed.
  ///
  /// Default implementation just keeps track of the total number of warnings
  /// and errors.
  virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, SourceLocation L,
                                const llvm::Twine &Msg,
                                llvm::ArrayRef<SourceRange> Ranges =
                                  llvm::ArrayRef<SourceRange>(),
                                llvm::ArrayRef<FixItHint> FixIts =
                                  llvm::ArrayRef<FixItHint>());
};

} // end namespace flang

#endif
