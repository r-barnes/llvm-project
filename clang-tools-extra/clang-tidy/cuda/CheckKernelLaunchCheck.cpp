//===--- CheckKernelLaunchCheck.cpp - clang-tidy --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CheckKernelLaunchCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include <iostream>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace cuda {

/// Matches a Stmt whose parent is a CompoundStmt, and which is directly
/// followed by a Stmt matching the inner matcher.
AST_MATCHER_P(Stmt, nextStmt, ast_matchers::internal::Matcher<Stmt>,
              InnerMatcher) {
  const CompoundStmt* containing_compound = nullptr;
  const Stmt* sibling = &Node;

  while (true) {
    //Look at my parents
    const auto& parents = Finder->getASTContext().getParents(*sibling);
    //Do I have parents?
    if ( parents.empty() ) {
      return false;
    }
    //Is my parent a statement?
    const auto pstmt = parents[0].get<Stmt>();
    if (!pstmt)
      return false;
    // pstmt->dump();
    //Is my parent a compound statement?
    if (isa<CompoundStmt>(pstmt)){
      containing_compound = parents[0].get<CompoundStmt>();
      break; //Yay - we found it!
    }
    //No. Then my parent might be the sibling, so I become my parent
    sibling = pstmt;
  }

  //There was no compound statement in which the kernel call could have a
  //sibling
  if(!containing_compound){
    return false;
  }

  const auto *I = llvm::find(containing_compound->body(), sibling);
  assert(I != containing_compound->body_end() && "C is parent of Node");
  if (++I == containing_compound->body_end())
    return false; // Node is last statement.

  return InnerMatcher.matches(**I, Finder, Builder);
}

void CheckKernelLaunchCheck::registerMatchers(MatchFinder *Finder) {
  const auto check_kernel = callExpr(callee(functionDecl(hasName("cudaGetLastError"))));
  Finder->addMatcher(cudaKernelCallExpr(unless(nextStmt(check_kernel))).bind("x"), this);
}

void CheckKernelLaunchCheck::check(const MatchFinder::MatchResult &Result) {
  // FIXME: Add callback implementation.
  const auto *S = Result.Nodes.getNodeAs<Expr>("x");

  diag(S->getExprLoc(), "Kernel lauch without accompanying launch check");
}

} // namespace cuda
} // namespace tidy
} // namespace clang
