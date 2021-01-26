//===--- HasDeviceGuardCheck.cpp - clang-tidy -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "HasDeviceGuardCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include <iostream>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace cuda {




class FunctionASTVisitor : public RecursiveASTVisitor<FunctionASTVisitor> {
    using Base = RecursiveASTVisitor<FunctionASTVisitor>;

 public:
    bool VisitCudaKernelCallExpr(CUDAKernelCallExpr *ckce){
      std::cerr<<"HI!!!!!"<<std::endl;
      return false;
    }

    bool TraverseCudaKernelCallExpr(CUDAKernelCallExpr *ckce){
      std::cerr<<"EEEEE!!!!!"<<std::endl;
      return false;
    }

    bool VisitDecl(Decl *d){
      d->dump();
      return true;
    }
};

void HasDeviceGuardCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(functionDecl(hasAnyParameter(hasType(recordDecl(matchesName("Tensor")))), hasDescendant(cudaKernelCallExpr())).bind("func"), this);
}

void HasDeviceGuardCheck::check(const MatchFinder::MatchResult &Result) {
  // FIXME: Add callback implementation.
  const auto *tensor_func_with_launch = Result.Nodes.getNodeAs<FunctionDecl>("x");
  // auto *dumpee = Result.Nodes.getNodeAs<FunctionDecl>("x");
  // dumpee->dump();

  tensor_func_with_launch->dump();

  // std::cerr<<"I found a thing!"<<std::endl;

  FunctionASTVisitor visitor;
  visitor.TraverseDecl(const_cast<FunctionDecl *>(tensor_func_with_launch));

  // if (!MatchedDecl->getIdentifier() || MatchedDecl->getName().startswith("awesome_"))
  //   return;
  // diag(MatchedDecl->getLocation(), "function %0 is insufficiently awesome")
  //     << MatchedDecl;
  // diag(MatchedDecl->getLocation(), "insert 'awesome'", DiagnosticIDs::Note)
  //     << FixItHint::CreateInsertion(MatchedDecl->getLocation(), "awesome_");
}

} // namespace cuda
} // namespace tidy
} // namespace clang
