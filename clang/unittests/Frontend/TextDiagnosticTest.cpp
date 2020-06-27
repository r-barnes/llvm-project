//===- unittests/Frontend/TextDiagnosticTest.cpp - ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/TextDiagnostic.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/LangOptions.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "gtest/gtest.h"

#include <string>

using namespace llvm;
using namespace clang;

namespace {

class TextDiagnosticExposer : public TextDiagnostic {
 public:
  using TextDiagnostic::TextDiagnostic;
  void emitIncludeLocation(FullSourceLoc Loc, PresumedLoc PLoc) override {
    return TextDiagnostic::emitIncludeLocation(Loc, PLoc);
  }
  void emitImportLocation(FullSourceLoc Loc, PresumedLoc PLoc, StringRef ModuleName) override {
    return TextDiagnostic::emitImportLocation(Loc, PLoc, ModuleName);
  }
  void emitBuildingModuleLocation(FullSourceLoc Loc, PresumedLoc PLoc, StringRef ModuleName) override {
    return TextDiagnostic::emitBuildingModuleLocation(Loc, PLoc, ModuleName);
  }
};



class TextDiagnosticTest : public ::testing::Test {
  IntrusiveRefCntPtr<vfs::InMemoryFileSystem> VFS;
  StringMap<std::string> RemappedFiles;
  std::shared_ptr<PCHContainerOperations> PCHContainerOpts;
  FileSystemOptions FSOpts;
  std::unique_ptr<ASTUnit> AST;
  SourceManager *sm;
  SourceLocation source_location;
  std::string OutString;
  LangOptions LangOpts;

public:
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts;
  FullSourceLoc full_source_loc;
  std::unique_ptr<raw_string_ostream> OS;

  void SetUp() override {
    ResetVFS();

    DiagOpts = new DiagnosticOptions();

    std::string MainName = "an/absolute/path/main.cpp";
    AddFile(MainName, R"cpp(
    #include "//./header1.h"
    int main() { return 0; }
    )cpp");

    AST = ParseAST(MainName);

    source_location = AST->getStartOfMainFileID();
    sm = &(AST->getSourceManager());
    full_source_loc = FullSourceLoc(source_location, *sm);

    OS.reset(new raw_string_ostream(OutString));
  }

  void TearDown() override {}

  TextDiagnosticExposer BuildTextDiagnosticExposer(){
    return TextDiagnosticExposer(*OS, LangOpts, &*DiagOpts);
  }

  void ResetVFS() {
    VFS = new vfs::InMemoryFileSystem();
    // We need the working directory to be set to something absolute,
    // otherwise it ends up being inadvertently set to the current
    // working directory in the real file system due to a series of
    // unfortunate conditions interacting badly.
    // What's more, this path *must* be absolute on all (real)
    // filesystems, so just '/' won't work (e.g. on Win32).
    VFS->setCurrentWorkingDirectory("//this/is/absolutely/");
  }

  void AddFile(const std::string &Filename, const std::string &Contents) {
    ::time_t now;
    ::time(&now);
    VFS->addFile(Filename, now, MemoryBuffer::getMemBufferCopy(Contents, Filename));
  }

  std::unique_ptr<ASTUnit> ParseAST(const std::string &EntryFile) {
    PCHContainerOpts = std::make_shared<PCHContainerOperations>();
    std::shared_ptr<CompilerInvocation> CI(new CompilerInvocation);
    CI->getFrontendOpts().Inputs.push_back(
      FrontendInputFile(EntryFile, FrontendOptions::getInputKindForExtension(
        llvm::sys::path::extension(EntryFile).substr(1))));

    CI->getTargetOpts().Triple = "i386-unknown-linux-gnu";

    IntrusiveRefCntPtr<DiagnosticsEngine>
      Diags(CompilerInstance::createDiagnostics(new DiagnosticOptions, new DiagnosticConsumer));

    FileManager *FileMgr = new FileManager(FSOpts, VFS);

    std::unique_ptr<ASTUnit> AST = ASTUnit::LoadFromCompilerInvocation(
        CI, PCHContainerOpts, Diags, FileMgr, false, CaptureDiagsKind::None,
        /*PrecompilePreambleAfterNParses=*/1);
    return AST;
  }
};



TEST_F(TextDiagnosticTest, EmitIncludeLocationHandlesAbsolutePaths) {
  DiagOpts->AbsolutePath = true;
  auto td = BuildTextDiagnosticExposer();
  td.emitIncludeLocation(full_source_loc, full_source_loc.getPresumedLoc(DiagOpts->ShowPresumedLoc));
  ASSERT_EQ(OS->str(), "In file included from //this/is/absolutely/an/absolute/path/main.cpp:1:\n");
}

TEST_F(TextDiagnosticTest, EmitIncludeLocationHandlesRelativePaths) {
  DiagOpts->AbsolutePath = false;
  auto td = BuildTextDiagnosticExposer();
  td.emitIncludeLocation(full_source_loc, full_source_loc.getPresumedLoc(DiagOpts->ShowPresumedLoc));
  ASSERT_EQ(OS->str(), "In file included from an/absolute/path/main.cpp:1:\n");
}


TEST_F(TextDiagnosticTest, emitImportLocationHandlesAbsolutePaths) {
  DiagOpts->AbsolutePath = true;
  auto td = BuildTextDiagnosticExposer();
  td.emitImportLocation(full_source_loc, full_source_loc.getPresumedLoc(DiagOpts->ShowPresumedLoc), "ModuleName");
  ASSERT_EQ(OS->str(), "In module 'ModuleName' imported from //this/is/absolutely/an/absolute/path/main.cpp:1:\n");
}

TEST_F(TextDiagnosticTest, emitImportLocationHandlesRelativePaths) {
  DiagOpts->AbsolutePath = false;
  auto td = BuildTextDiagnosticExposer();
  td.emitImportLocation(full_source_loc, full_source_loc.getPresumedLoc(DiagOpts->ShowPresumedLoc), "ModuleName");
  ASSERT_EQ(OS->str(), "In module 'ModuleName' imported from an/absolute/path/main.cpp:1:\n");
}


TEST_F(TextDiagnosticTest, emitBuildingModuleLocationHandlesAbsolutePaths) {
  DiagOpts->AbsolutePath = true;
  auto td = BuildTextDiagnosticExposer();
  td.emitBuildingModuleLocation(full_source_loc, full_source_loc.getPresumedLoc(DiagOpts->ShowPresumedLoc), "ModuleName");
  ASSERT_EQ(OS->str(), "While building module 'ModuleName' imported from //this/is/absolutely/an/absolute/path/main.cpp:1:\n");
}

TEST_F(TextDiagnosticTest, emitBuildingModuleLocationHandlesRelativePaths) {
  DiagOpts->AbsolutePath = false;
  auto td = BuildTextDiagnosticExposer();
  td.emitBuildingModuleLocation(full_source_loc, full_source_loc.getPresumedLoc(DiagOpts->ShowPresumedLoc), "ModuleName");
  ASSERT_EQ(OS->str(), "While building module 'ModuleName' imported from an/absolute/path/main.cpp:1:\n");
}

} // anonymous namespace
