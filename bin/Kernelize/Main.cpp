// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "KernelCodeGenVisitorMixin.hpp"
#include <iostream>
#include <macroni/Common/ParseAST.hpp>
#include <macroni/Conversion/Kernel/KernelRewriters.hpp>
#include <macroni/Translation/MacroniCodeGenVisitorMixin.hpp>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>
#include <mlir/Transforms/Passes.h>
#include <optional>
#include <pasta/AST/AST.h>
#include <vast/CodeGen/CodeGen.hpp>

int main(int argc, char **argv) {
    auto maybe_ast = pasta::parse_ast(argc, argv);
    if (!maybe_ast.Succeeded()) {
        std::cerr << maybe_ast.TakeError() << '\n';
        return EXIT_FAILURE;
    }
    auto pasta_ast = maybe_ast.TakeValue();

    // Register the MLIR dialects we will be lowering to
    mlir::DialectRegistry registry;
    registry.insert<
        vast::hl::HighLevelDialect,
        macroni::macroni::MacroniDialect,
        macroni::kernel::KernelDialect
    >();
    auto mctx = mlir::MLIRContext(registry);
    macroni::MacroniCodeGenContext cgctx(mctx, pasta_ast.UnderlyingAST(),
                                         pasta_ast);
    KernelCodeGen codegen(cgctx);

    // Generate the MLIR
    auto tu_decl = pasta_ast.UnderlyingAST().getTranslationUnitDecl();
    auto mod = codegen.emit_module(tu_decl);

    // Register conversions
    mlir::RewritePatternSet patterns(&mctx);
    patterns.add(macroni::kernel::rewrite_get_user)
        .add(macroni::kernel::rewrite_offsetof)
        .add(macroni::kernel::rewrite_container_of)
        .add(macroni::kernel::rewrite_rcu_dereference)
        .add(macroni::kernel::rewrite_smp_mb)
        .add(macroni::kernel::rewrite_list_for_each)
        .add(macroni::kernel::rewrite_rcu_read_unlock);

    // Apply the conversions.
    mlir::FrozenRewritePatternSet frozen_pats(std::move(patterns));
    mod->walk([&frozen_pats](mlir::Operation *op) {
        if (mlir::isa<
            macroni::macroni::MacroExpansion,
            vast::hl::ForOp,
            vast::hl::CallOp>(op)) {
            std::ignore = mlir::applyOpPatternsAndFold(op, frozen_pats);
        }}
    );
    // Print the result
    mod->print(llvm::outs());

    return EXIT_SUCCESS;
}
