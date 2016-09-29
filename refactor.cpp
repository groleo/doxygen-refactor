//    This file implements a tool that replaces qtools with STL
//
//    Usage:
//    refactor <cmake-output-dir> <file1> <file2> ...
//
//    Where <cmake-output-dir> is a CMake build directory in which a file named
//    compile_commands.json exists (enable -DCMAKE_EXPORT_COMPILE_COMMANDS in
//    CMake to get this output).
//
//    <file1> ... specify the paths of files in the CMake source tree.
//
//
//    http://clang.llvm.org/docs/LibASTMatchersReference.html
//    https://github.com/jiazhihao/clang/blob/master/unittests/ASTMatchers/ASTMatchersTest.cpp

#include <stddef.h>

#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"

#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"

#include <system_error>
#include <regex>
#include <unordered_set>

using namespace clang;
using namespace clang::ast_matchers;
using namespace llvm;
using clang::tooling::newFrontendActionFactory;
using clang::tooling::Replacement;
using clang::tooling::CompilationDatabase;

cl::opt<std::string>  BuildPath(cl::Positional, cl::desc("<build-path>"));
cl::list<std::string> SourcePaths(cl::Positional, cl::desc("<source0> [... <sourceN>]"), cl::OneOrMore);

std::unordered_set<const FieldDecl*> g_unique_fdecl;
std::unordered_set<const RecordDecl*> g_unique_recorddecl;

static std::string getText(const SourceManager &SourceManager,
                          SourceLocation StartSpellingLocation,
                          SourceLocation EndSpellingLocation) {
  if (!StartSpellingLocation.isValid() || !EndSpellingLocation.isValid()) {
    return std::string();
  }

  bool Invalid = true;
  const char *Text = SourceManager.getCharacterData(StartSpellingLocation, &Invalid);
  if (Invalid) {
    return std::string();
  }

  std::pair<FileID, unsigned> Start = SourceManager.getDecomposedLoc(StartSpellingLocation);
  std::pair<FileID, unsigned> End   = SourceManager.getDecomposedLoc(Lexer::getLocForEndOfToken(
        EndSpellingLocation, 0, SourceManager, LangOptions()));
  if (Start.first != End.first) {
    // Start and end are in different files.
    return std::string();
  }
  if (End.second < Start.second) {
    // Shuffling text with macros may cause this.
    return std::string();
  }

  return std::string(Text, End.second - Start.second);
}
////////////////////////////////////////////////////////////////////////////////
// Returns the text that makes up 'node' in the source.
// Returns an empty string if the text cannot be found.
////////////////////////////////////////////////////////////////////////////////
template <typename T>
static std::string getText(const SourceManager &SourceManager, const T &Node) {
  SourceLocation StartSpellingLocation = SourceManager.getSpellingLoc(Node.getLocStart());
  SourceLocation EndSpellingLocation   = SourceManager.getSpellingLoc(Node.getLocEnd());
  return getText(SourceManager, StartSpellingLocation, EndSpellingLocation);
}


////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
bool findNreplace(std::string& str, const std::string& rgx, const std::string& fmt, bool log=false) {
  std::regex _rgx(rgx);
  std::smatch _mtch;
  std::regex_search(str, _mtch, _rgx);

  if (_mtch.size())
  {
    if (log) {
      llvm::errs() << str << " ";
    }
    str = std::regex_replace (str,_rgx,fmt);
    if (log) {
      llvm::errs() << str << "\n";
    }
    return true;
  }
  return false;
}

class BaseMatcherCb : public ast_matchers::MatchFinder::MatchCallback {
public:
    BaseMatcherCb(tooling::Replacements *r) : Replace(r) {}
  protected:
    tooling::Replacements *Replace;
};


namespace qdict {
// O:- [ ] QDict <T> -> std::unordered_map<std::string, T*>
// O:  - [x] variable declaration QDictIterator
// O:  - [ ] QDictIterator<T> li(children) -> std::list<T*>::iterator li = children.begin()
class VarDeclIteratorCb : public BaseMatcherCb {
public:
    VarDeclIteratorCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto decl = result.Nodes.getNodeAs<VarDecl>("qdict::varDeclIterator");
      if (decl==nullptr) {
        llvm::errs() <<"Unable to get decl\n";
        return;
      }
      auto str = getText(*result.SourceManager,*decl);
      if (! findNreplace(str,"QDictIterator\\s*<\\s*(\\w+)\\s*>\\s*(\\w+)\\(\\*(.*)\\)","std::unordered_map<std::string, $1*>::iterator $2(@B$3->@Ebegin())") )
      if (! findNreplace(str,"QDictIterator\\s*<\\s*(\\w+)\\s*>\\s*(\\w+)\\((.*)\\)","std::unordered_map<std::string, $1*>::iterator $2(@B$3.@Ebegin())") )
      if (! findNreplace(str,"QDictIterator\\s*<\\s*(\\w+)\\s*>\\s*\\((.*)\\)","std::unordered_map<std::string, $1*>::iterator ($2->begin())") )
      if (! findNreplace(str,"QDictIterator\\s*<\\s*(\\w+)\\s*>","std::unordered_map<std::string, $1*>::iterator") )
      if (! findNreplace(str,"(\\w+)DictIterator (\\w+)\\(\\*(.*)\\)","std::unordered_map<std::string, $1*>::iterator $2(@B$3->@Ebegin())") )
      if (! findNreplace(str,"(\\w+)DictIterator (\\w+)\\((.*)\\)","std::unordered_map<std::string, $1*>::iterator $2(@B$3.@Ebegin())") )
      if (! findNreplace(str,"(\\w+)DictIterator","std::unordered_map<std::string, $1*>::iterator") )
        return;
      Replace->insert(Replacement(*result.SourceManager, decl, str));
    }
};

// O:  - [x] field declaration QList
class FieldDeclCb : public BaseMatcherCb {
public:
    FieldDeclCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto decl = result.Nodes.getNodeAs<FieldDecl>("qdict::fieldDecl");
      if (decl==nullptr) {
        llvm::errs() <<"Unable to get decl\n";
        return;
      }
      auto str = getText(*result.SourceManager,*decl);
      if (! findNreplace(str,"QDict<(\\w+)>","std::unordered_map<std::string, $1*>") ) return;
      Replacement rep(*result.SourceManager, decl, str);
      if (g_unique_fdecl.find(decl) == g_unique_fdecl.end()) {
        Replace->insert(rep);
        g_unique_fdecl.insert(decl);
      }
    }
};
}; // namespace qdict


namespace qlist {

// O:- [ ] QList <T> -> std::list<T*>
// O:  - [x] class inheriting QList
class InheritCb : public BaseMatcherCb {
public:
    InheritCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      auto decl = result.Nodes.getNodeAs<CXXRecordDecl>("inheritsQList");
      if (decl==nullptr) {
        return;
      }
      if (!decl->hasDefinition()) return; // this is needed so bases_begin doesn't crash
      auto it = decl->bases_begin();
      for (; it && it != decl->bases_end(); ++it) {
        auto str = getText(*result.SourceManager,it->getLocStart(),it->getLocEnd()) ;
        if (! findNreplace(str,"QList<(\\w+)>","std::list<$1*>") ) return;
        Replace->insert(Replacement(*result.SourceManager, it, str));
      }
    }
};

// O:  - [x] variable declaration QList
class VarDeclCb : public BaseMatcherCb {
public:
    VarDeclCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto decl = result.Nodes.getNodeAs<VarDecl>("varDecl");
      if (decl==nullptr) {
        llvm::errs() <<"Unable to get decl\n";
        return;
      }
      auto str = getText(*result.SourceManager,*decl);
      if (! findNreplace(str,"QList<(\\w+)>","std::list<$1*>") ) return;
      Replace->insert(Replacement(*result.SourceManager, decl, str));
    }
};

// O:  - [x] field declaration QList
class FieldDeclCb : public BaseMatcherCb {
public:
    FieldDeclCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto decl = result.Nodes.getNodeAs<FieldDecl>("qlist::fieldDecl");
      if (decl==nullptr) {
        llvm::errs() <<"Unable to get decl\n";
        return;
      }
      auto str = getText(*result.SourceManager,*decl);
      if (! findNreplace(str,"QList<(\\w+)>","std::list<$1*>") ) return;
      Replacement rep(*result.SourceManager, decl, str);
      if (g_unique_fdecl.find(decl) == g_unique_fdecl.end()) {
        Replace->insert(rep);
        g_unique_fdecl.insert(decl);
      }
    }
};

// O:  - [x] parameter declaration QList
class ParmVarDeclCb : public BaseMatcherCb {
public:
    ParmVarDeclCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto decl = result.Nodes.getNodeAs<ParmVarDecl>("parmVarDecl");
      if (decl==nullptr) {
        llvm::errs() <<"Unable to get decl\n";
        return;
      }
      auto str = getText(*result.SourceManager,*decl);
      if (! findNreplace(str,"QList<(\\w+)>","std::list<$1*>") ) return;
      Replace->insert(Replacement(*result.SourceManager, decl, str));
    }
};

// O:  - [x] getFirst() -> std::list::front()
class GetFirstCb : public BaseMatcherCb {
public:
    GetFirstCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      std::string m="getFirst";
      const auto call = result.Nodes.getNodeAs<CXXMemberCallExpr>(m);
      if (call==nullptr) {
        llvm::errs() << "unable to get " << m << "\n";
        return;
      }
      const auto callee = call->getCallee();
      auto str = getText(*result.SourceManager,*callee);
      if (! findNreplace(str,m,"front") ) return;
      Replace->insert(Replacement(*result.SourceManager, callee, str));
    }
};

// O:  - [x] getLast() -> std::list::end()
class GetLastCb : public BaseMatcherCb {
public:
    GetLastCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      std::string m="getLast";
      const auto call = result.Nodes.getNodeAs<CXXMemberCallExpr>(m);
      if (call==nullptr) {
        llvm::errs() << "unable to get " << m << "\n";
        return;
      }
      const auto callee = call->getCallee();
      auto str = getText(*result.SourceManager,*callee);
      if (! findNreplace(str,m,"back") ) return;
      Replace->insert(Replacement(*result.SourceManager, callee, str));
    }
};

// O:  - [x] isEmpty() -> std::list::empty()
class IsEmptyCb : public BaseMatcherCb {
public:
    IsEmptyCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      std::string m="isEmpty";
      const auto call = result.Nodes.getNodeAs<CXXMemberCallExpr>(m);
      if (call==nullptr) {
        llvm::errs() << "unable to get " << m << "\n";
        return;
      }
      const auto callee = call->getCallee();
      auto str = getText(*result.SourceManager,*callee);
      if (! findNreplace(str,m,"empty") ) return;
      Replace->insert(Replacement(*result.SourceManager, callee, str));
    }
};

// O:  - [x] count() -> std::list::size()
class CountCb : public BaseMatcherCb {
public:
    CountCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      std::string m="count";
      const auto call = result.Nodes.getNodeAs<CXXMemberCallExpr>(m);
      if (call==nullptr) {
        llvm::errs() << "unable to get " << m << "\n";
        return;
      }
      const auto callee = call->getCallee();
      auto str = getText(*result.SourceManager,*callee);
      if (! findNreplace(str,m,"size") ) return;
      Replace->insert(Replacement(*result.SourceManager, callee, str));
    }
};

class FieldSetAutoDeleteTrueCb : public BaseMatcherCb {
public:
    FieldSetAutoDeleteTrueCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto call = result.Nodes.getNodeAs<CXXConstructorDecl>("Field_setAutoDeleteTRUE");
      if (call==nullptr) {
        return;
      }
      const auto fdecl = result.Nodes.getNodeAs<FieldDecl>("C");
      if (fdecl==nullptr) {
        return;
      }
      auto callStr = getText(*result.SourceManager,*call);
      if (callStr.find("setAutoDelete(TRUE)") == std::string::npos) {
        return;
      }
      if (g_unique_fdecl.find(fdecl) != g_unique_fdecl.end()) {
        return;
      }
      auto fdeclStr = getText(*result.SourceManager,*fdecl);
      // place to use shared_ptr
      if (! findNreplace(fdeclStr,"QList<(\\w+)>","std::list<$1*>") ) return;
      Replace->insert(Replacement(*result.SourceManager, fdecl, fdeclStr));
      g_unique_fdecl.insert(fdecl);
    }
};

// O:  - [ ] QList->setAutoDelete(TRUE) -> unique_ptr
// O:    - [ ] BUG: setAutoDelete called in template classes is not matched
class SetAutoDeleteTrueCb : public BaseMatcherCb {
public:
    SetAutoDeleteTrueCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto call = result.Nodes.getNodeAs<CXXMemberCallExpr>("setAutoDeleteTRUE");
      if (call==nullptr) {
        return;
      }
      Replace->insert(Replacement(*result.SourceManager, call, ""));

      const auto thisDecl = result.Nodes.getNodeAs<VarDecl>("thisDecl");
      if (thisDecl==nullptr) {
        return;
      }
      auto callStr = getText(*result.SourceManager,*call);
      auto thisDeclStr = getText(*result.SourceManager,*thisDecl);
      // place to use shared_ptr
      if (! findNreplace(thisDeclStr,"QList<(\\w+)>","std::list<$1*>") ) return;
      Replace->insert(Replacement(*result.SourceManager, thisDecl, thisDeclStr));
    }
};

// O:    - [x] append(x) -> std::list::push_back(std::make_unique(x))
class AppendCb : public BaseMatcherCb {
public:
    AppendCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto varDecl = result.Nodes.getNodeAs<Stmt>("thisDeclAppend");
      if (varDecl == nullptr) {
        return;
      }
      const auto call = result.Nodes.getNodeAs<CXXMemberCallExpr>("append");
      if (call == nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*call);
      if (! findNreplace(str,"append\\((.*)\\)","push_back($1)") ) return;
      Replace->insert(Replacement(*result.SourceManager, call, str));
    }
};

// O:    - [x] prepend(x) -> std::list::push_front(std::make_unique(x))
class PrependCb : public BaseMatcherCb {
public:
    PrependCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto varDecl = result.Nodes.getNodeAs<Stmt>("thisDeclPrepend");
      if (varDecl == nullptr) {
        return;
      }
      const auto call = result.Nodes.getNodeAs<CXXMemberCallExpr>("prepend");
      if (call==nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*call);
      if (! findNreplace(str,"prepend\\((.*)\\)","push_front($1)") ) return;
      Replace->insert(Replacement(*result.SourceManager, call, str));
    }
};

// O:  - [ ] return ref: QList<T> & cxxMethodDecl()
// O:  - [ ] return ptr: QList<T> * cxxMethodDecl()
// O:  - [ ] return obj: QList<T>   cxxMethodDecl()
// O:  - [x] return ref: QList<T> & functionDecl()
// O:  - [x] return ptr: QList<T> * functionDecl()
// O:  - [x] return obj: QList<T>   functionDecl()
class ReturnCb : public BaseMatcherCb {
public:
    ReturnCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto fdecl = result.Nodes.getNodeAs<FunctionDecl>("returnQList");
      if (fdecl==nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*fdecl);
      if (! findNreplace(str,"QList<(\\w+)>","std::list<$1*>") ) return;
      Replace->insert(Replacement(*result.SourceManager, fdecl, str));
    }
};

// O:  - [x] new expression: new QList<T>
class NewExprCb : public BaseMatcherCb {
public:
    NewExprCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto cxxNewExpr = result.Nodes.getNodeAs<CXXNewExpr>("qlist::cxxNewExpr");
      if (cxxNewExpr==nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*cxxNewExpr);
      if (! findNreplace(str,"QList<(\\w+)>","std::list<$1*>") ) return;
      Replace->insert(Replacement(*result.SourceManager, cxxNewExpr, str));
    }
};

// O:  - [x] QList<T> constructor
class ConstructExprCb : public BaseMatcherCb {
public:
    ConstructExprCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto cxxConstructExpr = result.Nodes.getNodeAs<CXXConstructExpr>("qlist::cxxConstructExpr");
      if (cxxConstructExpr==nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*cxxConstructExpr);
      if ( !findNreplace(str,"QList<(\\w+)>","std::list<$1*>"))
        return;
      Replace->insert(Replacement(*result.SourceManager, cxxConstructExpr, str));
    }
};

// O:  - [ ] remove(item) -> ?
// O:  - [ ] remove(index) -> ?
// O:  - [ ] findRef(item) -> ?

////////////////////////////////////////////////////////////////////////////////
// O:- [ ] QListIterator <T> -> std::list<T*>::iterator
// O:  - [x] class inheriting QListIterator
class InheritsIteratorCb : public BaseMatcherCb {
public:
    InheritsIteratorCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      auto decl = result.Nodes.getNodeAs<CXXRecordDecl>("inheritsQListIterator");
      if (decl==nullptr) {
        return;
      }
      if (!decl->hasDefinition()) return; // this is needed so bases_begin doesn't crash
      auto it = decl->bases_begin();
      for (; it && it != decl->bases_end(); ++it) {
        auto str = getText(*result.SourceManager,it->getLocStart(),it->getLocEnd()) ;
        if (! findNreplace(str,"QListIterator<(\\w+)>","std::list<$1*>::iterator") ) return;
        Replace->insert(Replacement(*result.SourceManager, it, str));
      }
    }
};

// O:  - [x] variable declaration QListIterator
// O:  - [ ] QListIterator<T> li(children) -> std::list<T*>::iterator li = children.begin()
class VarDeclIteratorCb : public BaseMatcherCb {
public:
    VarDeclIteratorCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto decl = result.Nodes.getNodeAs<VarDecl>("varDeclIterator");
      if (decl==nullptr) {
        llvm::errs() <<"Unable to get decl\n";
        return;
      }
      auto str = getText(*result.SourceManager,*decl);
      if (! findNreplace(str,"QListIterator\\s*<\\s*(\\w+)\\s*>\\s*(\\w+)\\(\\*(.*)\\)","std::list<$1*>::iterator $2(@B$3->@Ebegin())") )
      if (! findNreplace(str,"QListIterator\\s*<\\s*(\\w+)\\s*>\\s*(\\w+)\\((.*)\\)","std::list<$1*>::iterator $2(@B$3.@Ebegin())") )
      if (! findNreplace(str,"QListIterator\\s*<\\s*(\\w+)\\s*>\\s*\\((.*)\\)","std::list<$1*>::iterator ($2->begin())") )
      if (! findNreplace(str,"QListIterator\\s*<\\s*(\\w+)\\s*>","std::list<$1*>::iterator") )
      if (! findNreplace(str,"(\\w+)ListIterator (\\w+)\\(\\*(.*)\\)","std::list<$1*>::iterator $2(@B$3->@Ebegin())") )
      if (! findNreplace(str,"(\\w+)ListIterator (\\w+)\\((.*)\\)","std::list<$1*>::iterator $2(@B$3.@Ebegin())") )
      if (! findNreplace(str,"(\\w+)ListIterator","std::list<$1*>::iterator") )
        return;
      Replace->insert(Replacement(*result.SourceManager, decl, str));
    }
};

class IteratorCb : public BaseMatcherCb {
public:
    IteratorCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto fdecl = result.Nodes.getNodeAs<CXXConstructExpr>("qlistIterator");
      if (fdecl==nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*fdecl);
      if (! findNreplace(str, "QListIterator<(\\w+)>\\((\\w+)\\)", "std::list<$1*>::iterator(@B$2.@Ebegin())") )
      if (! findNreplace(str,"(\\w+)ListIterator (\\w+)\\((.*)\\)","std::list<$1*>::iterator $2(@B$3.@Ebegin())") )
        return;
      Replace->insert(Replacement(*result.SourceManager, fdecl, str));
    }
};

class ForStmtIteratorCb : public BaseMatcherCb {
public:
    ForStmtIteratorCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto fdecl = result.Nodes.getNodeAs<ForStmt>("forStmtIterator");
      if (fdecl==nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*fdecl);
      // for (ali.toFirst();!hasDocs && (a=ali.current());++ali)
      // for (ali.toFirst();!hasDocs && (ali!=this->end() && a=*ali);++ali)
      //-----
      // for (ali.toFirst();!hasDocs && (a=ali.current());++ali)
      // for (;(a=ali.current());++ali)
      if (! findNreplace(str, "\\(.*\\.toFirst\\(\\);(.*)\\((\\w+)=(\\w+).current\\(\\)\\);", "(; $1 (@X$2,$3@Y); ") )
      if (! findNreplace(str, "\\((\\w+)=(\\w+).current\\(\\)\\)", "(@X$1,$2@Y)") )
        return;
      Replace->insert(Replacement(*result.SourceManager, fdecl, str));
    }
};

// O:  - [x] return ref: QListIterator<T> & cxxMethodDecl()
// O:  - [x] return ptr: QListIterator<T> * cxxMethodDecl()
// O:  - [x] return obj: QListIterator<T>   cxxMethodDecl()
// O:  - [ ] return ref: QListIterator<T> & functionDecl()
// O:  - [ ] return ptr: QListIterator<T> * functionDecl()
// O:  - [ ] return obj: QListIterator<T>   functionDecl()
class ReturnIteratorCb : public BaseMatcherCb {
public:
    ReturnIteratorCb(tooling::Replacements *r) : BaseMatcherCb(r) {}

    virtual void run(const ast_matchers::MatchFinder::MatchResult &result) {
      const auto decl = result.Nodes.getNodeAs<CXXMethodDecl>("returnQListIterator");
      if (decl==nullptr) {
        return;
      }
      auto str = getText(*result.SourceManager,*decl);

      std::string in = str;
      std::regex re("QListIterator\\s*<\\s*(\\w+)\\s*>\\s*\\((.*)\\)");
      std::smatch m;
      std::string out;

      while (std::regex_search(in, m, re))
      {
        out += m.prefix();
        std::regex re("QListIterator\\s*<\\s*(\\w+)\\s*>\\s*\\(\\*(.*)\\)");
        out += std::regex_replace(m[0].str(), re, "std::list<$1*>::iterator ($2->begin())");
        in = m.suffix();
      }
      out += in;
      in = out;
      re = "QListIterator\\s*<\\s*(\\w+)\\s*>";
      out = "";

      while (std::regex_search(in, m, re))
      {
        out += m.prefix();
        std::regex re("QListIterator\\s*<\\s*(\\w+)\\s*>");
        out += std::regex_replace(m[0].str(), re, "std::list<$1*>::iterator");
        in = m.suffix();
      }
      out += in;
      Replace->insert(Replacement(*result.SourceManager, decl, out));
    }
};
}; // namespace qlist


////////////////////////////////////////////////////////////////////////////////
// O:- [ ] QIntDict <T> -> std::map<T*>
// O:  - [ ] QIntDict<T> -> std::unordered_map<long, T*>
// O:  - [ ] constructor QIntDict<T> (N) -> std::unordered_map<T*>::reserve(N)
// O:  - [ ] QIntDictIterator(9)
// O:  - [ ] classes inheriting QIntDict(4)
// O:  - [ ] QIntDict::setAutoDelete(TRUE) -> unique_ptr
// O:- [ ] QDict
// O:  - [ ] QDict<T> -> std::unordered_map<std::string, T*>
// O:  - [ ] QDictIterator<T> -> std::unordered_map<std::string, T*>::iterator
// O:  - [ ] constructor QDict<T>(N) -> std::unordered_map<>::reserve(N)
// O:  - [ ] QDict<T>::resize(N) -> std::unordered_map<T*>::reserve(N)
// O:- [ ] QSDict
// O:- [ ] QStack
// O:- [ ] QArray
// O:- [ ] QMap
// O:- [ ] QStringList
// O:- [ ] QVector
// O:- [ ] QCache
// O:  - [ ] QCacheIterator


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal("");

  std::unique_ptr<CompilationDatabase> Compilations(
      tooling::FixedCompilationDatabase::loadFromCommandLine(argc, argv));

  cl::ParseCommandLineOptions(argc, argv);
  if (!Compilations) {
    std::string ErrorMessage;
    Compilations =
      CompilationDatabase::loadFromDirectory(BuildPath, ErrorMessage);
    if (!Compilations)
      llvm::report_fatal_error(ErrorMessage);
  }

  tooling::RefactoringTool Tool(*Compilations, SourcePaths);

  ast_matchers::MatchFinder Finder;

  auto recordDeclQList = cxxRecordDecl(isSameOrDerivedFrom(hasName("QList")));

  qlist::InheritCb cb_i(&Tool.getReplacements());
  Finder.addMatcher(
      id("inheritsQList",
        recordDeclQList
        )
      ,&cb_i);

  qlist::ReturnCb cb5(&Tool.getReplacements());
  Finder.addMatcher(
      id("returnQList",
        functionDecl(returns( anyOf(pointsTo(recordDeclQList), references(recordDeclQList),hasDeclaration(recordDeclQList) )))
        )
      ,&cb5);

  qlist::VarDeclCb cb1(&Tool.getReplacements());
  Finder.addMatcher(
      id("varDecl",
        varDecl(anyOf(hasType(recordDeclQList), hasType(pointsTo(recordDeclQList))))
        )
      ,&cb1);

  // function params refs
#if 0
  Parmqlist::VarDeclCb cb3(&Tool.getReplacements());
  Finder.addMatcher(
      id("parmVarDecl",
        parmVarDecl(hasType(references(cxxRecordDecl(isSameOrDerivedFrom(hasName("QList"))))))
        )
      ,&cb3);
#endif

  qlist::IsEmptyCb cb2(&Tool.getReplacements());
  Finder.addMatcher(
      id("isEmpty",
        cxxMemberCallExpr(callee(memberExpr(member(hasName("isEmpty")))), thisPointerType(cxxRecordDecl(isSameOrDerivedFrom("QList"),isTemplateInstantiation() )))
        )
      ,&cb2);
  
  qlist::CountCb cb2_2(&Tool.getReplacements());
  Finder.addMatcher(
      id("count",
        cxxMemberCallExpr(callee(memberExpr(member(hasName("count")))), thisPointerType(cxxRecordDecl(isSameOrDerivedFrom("QList"),isTemplateInstantiation() )))
        )
      ,&cb2_2);

  qlist::GetFirstCb cb21(&Tool.getReplacements());
  Finder.addMatcher(
      id("getFirst",
        cxxMemberCallExpr(callee(memberExpr(member(hasName("getFirst")))), thisPointerType(cxxRecordDecl(hasName("QList"),isTemplateInstantiation() )))
        )
      ,&cb21);

  qlist::GetLastCb cb22(&Tool.getReplacements());
  Finder.addMatcher(
      id("getLast",
        cxxMemberCallExpr(callee(memberExpr(member(hasName("getLast")))), thisPointerType(cxxRecordDecl(hasName("QList"),isTemplateInstantiation() )))
        )
      ,&cb22);

  // match: QList::setAutoDelete(TRUE)
  // this is needed so I can use std::unique_ptr
  qlist::FieldSetAutoDeleteTrueCb cb4(&Tool.getReplacements());
  Finder.addMatcher(
      id("Field_setAutoDeleteTRUE",
        cxxConstructorDecl(forEachConstructorInitializer(forField( fieldDecl(hasType(namedDecl(hasName("QList")) )).bind("C")  )) )
        // TODO: also catch hasType(pointsTo(namedDecl ...
      )
      ,&cb4);
   qlist::SetAutoDeleteTrueCb cb4_1(&Tool.getReplacements());
   Finder.addMatcher(
     id("setAutoDeleteTRUE",
       cxxMemberCallExpr( callee(memberExpr(member(hasName("setAutoDelete")))), thisPointerType(cxxRecordDecl(isSameOrDerivedFrom(hasName("QList")))))
       //cxxMemberCallExpr( callee(memberExpr(member(hasName("setAutoDelete")))), hasAnyArgument( declRefExpr( to( namedDecl(hasName("TRUE"))))),on( declRefExpr( to( id("thisDecl",varDecl(anything()))))), thisPointerType(recordDeclQList))
       )
     ,&cb4_1);

   qlist::FieldDeclCb cb11(&Tool.getReplacements());
   Finder.addMatcher(
     id("qlist::fieldDecl",
       fieldDecl(anyOf( hasType(pointsTo(recordDeclQList) )  ,  hasType(recordDeclQList)))
       )
     ,&cb11);

   qlist::AppendCb cb7(&Tool.getReplacements());
   Finder.addMatcher(
       id("append",
         cxxMemberCallExpr( callee(memberExpr(member(hasName("append")))), on(id("thisDeclAppend",expr())), thisPointerType( recordDeclQList ))
         )
       ,&cb7);

   qlist::PrependCb cb8(&Tool.getReplacements());
   Finder.addMatcher(
       id("prepend",
         cxxMemberCallExpr( callee(memberExpr(member(hasName("prepend")))), on( id("thisDeclPrepend",stmt(anything()))), thisPointerType( recordDeclQList))
         )
       ,&cb8);

   qlist::ConstructExprCb cb100(&Tool.getReplacements());
   Finder.addMatcher(
       id("qlist::cxxConstructExpr",
        cxxConstructExpr(hasType(namedDecl(hasName("QList"))))
        )
       ,&cb100);

   qlist::NewExprCb cb99(&Tool.getReplacements());
   Finder.addMatcher(
       id("qlist::cxxNewExpr",
         cxxNewExpr(hasType(qualType(pointsTo(namedDecl(hasName("QList"))))))
         )
       ,&cb99);


  ///////////////////////////////////////////////////////////////////
  //      Iterators
  ///////////////////////////////////////////////////////////////////

  auto recordDeclQListIterator = cxxRecordDecl(isSameOrDerivedFrom(hasName("QListIterator")));

  qlist::InheritsIteratorCb cb_ii(&Tool.getReplacements());
  Finder.addMatcher(
      id("inheritsQListIterator",
        recordDeclQListIterator
        )
      ,&cb_ii);

  qlist::VarDeclIteratorCb cb1it(&Tool.getReplacements());
  Finder.addMatcher(
      id("varDeclIterator",
        varDecl(anyOf(hasType(recordDeclQListIterator), hasType(pointsTo(recordDeclQListIterator))))
        )
      ,&cb1it);

   qlist::IteratorCb cb6(&Tool.getReplacements());
   Finder.addMatcher(
       id("qlistIterator",
         cxxConstructExpr(hasType(recordDeclQListIterator))
         )
       ,&cb6);


  qlist::ReturnIteratorCb it_cb5(&Tool.getReplacements());
  Finder.addMatcher(
      id("returnQListIterator",
        cxxMethodDecl(returns( anyOf(pointsTo(recordDeclQListIterator), references(recordDeclQListIterator), hasDeclaration(recordDeclQListIterator)  )))
        )
      ,&it_cb5);

   qlist::ForStmtIteratorCb cb66(&Tool.getReplacements());
   Finder.addMatcher(
       id("forStmtIterator",
         // cxxMemberCallExpr(callee(memberExpr(member(hasName("current")))),  thisPointerType(cxxRecordDecl(isSameOrDerivedFrom("QListIterator")))))
         forStmt( anyOf( hasLoopInit(cxxMemberCallExpr(callee(memberExpr(member(hasName("toFirst")))),  thisPointerType(cxxRecordDecl(isSameOrDerivedFrom("QListIterator")))))
                       ,  hasCondition(implicitCastExpr(   ) )
                       )
                )
         )
       ,&cb66);
  ///////////////////
  auto recordDeclQDict = cxxRecordDecl(isSameOrDerivedFrom(hasName("QDict")));
  auto recordDeclQDictIterator = cxxRecordDecl(isSameOrDerivedFrom(hasName("QDictIterator")));
  qdict::VarDeclIteratorCb dict1(&Tool.getReplacements());
  Finder.addMatcher(
      id("qdict::varDeclIterator",
        varDecl(anyOf(hasType(recordDeclQDictIterator), hasType(pointsTo(recordDeclQDictIterator))))
        )
      ,&dict1);

  qdict::FieldDeclCb qb11(&Tool.getReplacements());
   Finder.addMatcher(
     id("qdict::fieldDecl",
       fieldDecl(anyOf( hasType(pointsTo(recordDeclQDict) )  ,  hasType(recordDeclQDict)))
       )
     ,&qb11);

   return Tool.runAndSave(newFrontendActionFactory(&Finder).get());
}
