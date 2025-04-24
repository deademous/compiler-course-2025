#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class PrintUserTypeVisitor final
  : public clang::RecursiveASTVisitor<PrintUserTypeVisitor> {
public:
  explicit PrintUserTypeVisitor(clang::ASTContext *context) {}
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *Record) {
    if (!Record->isThisDeclarationADefinition() || Record->isImplicit()) {
      return true;
    }
    if (Record->isLocalClass()) {
      return true;
    }
    llvm::raw_ostream &OS = llvm::outs();
    OS << Record->getNameAsString();
    bool firstBase = true;
    if (Record->getNumBases() > 0) {
      OS << " -> ";
      for (const clang::CXXBaseSpecifier &BaseSpec : Record->bases()) {
        if (!firstBase) {
          OS << ", ";
        }
        const clang::RecordDecl* BaseRecordDecl = BaseSpec.getType()->getAsRecordDecl();
        if (BaseRecordDecl) {
          OS << BaseRecordDecl->getNameAsString();
        } else {
          OS << BaseSpec.getType().getAsString();
        }
        firstBase = false;
      }
    }
    OS << "\n";
    OS << "|_Fields\n";
    bool hasFields = false;
    for (const clang::FieldDecl *Field : Record->fields()) {
      hasFields = true;
      OS << "| |_ ";
      OS << Field->getNameAsString() << " (";
      OS << Field->getType().getAsString() << "|";
      switch (Field->getAccess()) {
        case clang::AS_public:    OS << "public"; break;
        case clang::AS_protected: OS << "protected"; break;
        case clang::AS_private:   OS << "private"; break;
        default:                  OS << "none"; break;
      }
      OS << ")\n";
    }
    if (hasFields) {
      OS << "|\n";
    }
    OS << "|_Methods\n";
    bool hasMethods = false;
    for (const clang::CXXMethodDecl *Method : Record->methods()) {
      if (Method->isImplicit() || llvm::isa<clang::CXXDestructorDecl>(Method)) {
        continue;
      }
      hasMethods = true;
      OS << "| |_ ";
      OS << Method->getNameAsString() << " (";
      OS << Method->getReturnType().getAsString() << "(";
      unsigned numParams = Method->getNumParams();
      for (unsigned i = 0; i < numParams; ++i) {
        OS << Method->getParamDecl(i)->getType().getAsString();
        if (i < numParams - 1) {
          OS << ", ";
        }
      }
      OS << ")";
      OS << "|";
      switch (Method->getAccess()) {
        case clang::AS_public:    OS << "public"; break;
        case clang::AS_protected: OS << "protected"; break;
        case clang::AS_private:   OS << "private"; break;
        default:                  OS << "none"; break;
      }
      if (Method->isPure()) {
        OS << "|virtual|pure";
      }
      else if (Method->hasAttr<clang::OverrideAttr>()) {
        OS << "|override";
      }
      else if (Method->isVirtual()) {
        OS << "|virtual";
      }
      OS << ")\n";
    }
    OS << "\n";
    return true;
  }
};

class PrintUserTypeConsumer final : public clang::ASTConsumer {
private:
  PrintUserTypeVisitor Visitor;

public:
  explicit PrintUserTypeConsumer(clang::ASTContext *context) : Visitor(context) {}
  void HandleTranslationUnit(clang::ASTContext &context) override {
    Visitor.TraverseDecl(context.getTranslationUnitDecl());
  }
};

class PrintUserTypeAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<PrintUserTypeConsumer>(&ci.getASTContext());
  }
  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

}
static clang::FrontendPluginRegistry::Add<PrintUserTypeAction>
  X("PrintUserTypeInfo",
    "Prints info about user types (fields, methods, bases)");
