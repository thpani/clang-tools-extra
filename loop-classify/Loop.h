#ifndef LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_H_
#define LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_H_

#include <string>       // std::string
#include <sstream>      // std::stringstream

using namespace clang;

// Returns the text that makes up 'node' in the source.
// Returns an empty string if the text cannot be found.
template <typename T>
static std::string getText(const SourceManager &SourceManager, const T &Node) {
  clang::SourceLocation StartSpellingLocatino =
      SourceManager.getSpellingLoc(Node.getLocStart());
  clang::SourceLocation EndSpellingLocation =
      SourceManager.getSpellingLoc(Node.getLocEnd());
  if (!StartSpellingLocatino.isValid() || !EndSpellingLocation.isValid()) {
    return std::string();
  }
  bool Invalid = true;
  const char *Text =
      SourceManager.getCharacterData(StartSpellingLocatino, &Invalid);
  if (Invalid) {
    return std::string();
  }
  std::pair<clang::FileID, unsigned> Start =
      SourceManager.getDecomposedLoc(StartSpellingLocatino);
  std::pair<clang::FileID, unsigned> End =
      SourceManager.getDecomposedLoc(clang::Lexer::getLocForEndOfToken(
          EndSpellingLocation, 0, SourceManager, clang::LangOptions()));
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

static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
  size_t start_pos = 0;
  while((start_pos = str.find(from, start_pos)) != std::string::npos) {
          str.replace(start_pos, from.length(), to);
          start_pos += to.length();
  }
}

static std::string escape(std::string &str) {
  std::string result = str;
  replaceAll(result, "\\", "\\\\");
  replaceAll(result, "\"", "\\\"");
  /* replaceAll(result, "\n", "\\n"); */
  return result;
}

class LoopInfo {
  public:
    LoopInfo(const Stmt *LoopStmt, const ASTContext *Context, const SourceManager *SM) {
      llvm::FoldingSetNodeID ID;
      LoopStmt->Profile(ID, *Context, true);
      Hash = ID.ComputeHash();
      const SourceLocation Loc = LoopStmt->getLocStart();
      FileName = SM->getFilename(Loc);
      LineNumber = SM->getExpansionLineNumber(Loc);
      SourceCode = getText(*SM, *LoopStmt);
    }
    static const std::string DumpSQLCreate() {
      std::stringstream sstm;
      sstm << "DROP TABLE IF EXISTS loops;\n"
           << "CREATE TABLE loops (id INTEGER, filename TEXT, linenumber TEXT, sourcecode TEXT);\n";
      return sstm.str();
    }
    const std::string DumpSQL() {
      std::stringstream sstm;
      sstm << "("
           << Hash << ","
           << "\"" << FileName << "\","
           << "\"" << LineNumber << "\","
           << "\"" << escape(SourceCode) << "\""
           << ")";
      return sstm.str();
    }
  private:
    std::string FileName, SourceCode;
    int LineNumber;
    unsigned Hash;
};

#endif // LLVM_TOOLS_CLANG_TOOLS_EXTRA_LOOP_CLASSIFY_LOOP_H_
