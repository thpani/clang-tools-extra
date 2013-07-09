#pragma once

#include <sstream>
#include <algorithm>
#include <vector>
#include <stack>

#include "clang/AST/ASTContext.h"

using namespace clang;
using namespace clang::tooling;

enum ClassificationKind {
  Fail,
  Success,
  Unknown
};

static std::string classificationKindToString(const ClassificationKind Kind) {
  return std::string(Kind == Fail ? "!" : (Kind == Unknown ? "?" : ""));
}

static std::string reasonToString(const std::string Marker, const std::string Suffix) {
  return Marker + (Suffix.size() > 0 ? "-" : "") + Suffix;
}

static std::string reasonToString(const ClassificationKind Kind, const std::string Marker, const std::string Suffix) {
  return classificationKindToString(Kind) + Marker + (Suffix.size() > 0 ? "-" : "") + Suffix;
}

class checkerror {
  public:
    checkerror(const ClassificationKind Kind, const std::string Marker, const std::string Suffix) :
      reason(reasonToString(Kind, Marker, Suffix)) {}

    const std::string what() const throw() {
      return reason;
    }
  private:
    const std::string reason;
};


class LoopClassifier {
  public:
    static void classify(const NaturalLoop* Loop, const ClassificationKind Kind, const std::string Marker, const int Count) {
      std::stringstream sstm;
      sstm << Count;
      classify(Loop, Kind, Marker, sstm.str());
    }
    static void classify(const NaturalLoop* Loop, const ClassificationKind Kind, const std::string Marker, const std::string Suffix="") {
      std::string S = reasonToString(Kind, Marker, Suffix);
      Classifications[Loop->getUnsliced()].insert(S);
    }
    /* overloads for success */
    static void classify(const NaturalLoop* Loop, const std::string Marker, const int Count) {
      std::stringstream sstm;
      sstm << Count;
      classify(Loop, Marker, sstm.str());
    }
    static void classify(const NaturalLoop* Loop, const std::string Marker, const std::string Suffix="") {
      std::string S = reasonToString(Marker, Suffix);
      Classifications[Loop->getUnsliced()].insert(S);
    }

    static bool hasClass(const NaturalLoop* Loop, const std::string WhichClass) {
      auto Classes = Classifications[Loop->getUnsliced()];
      for (auto Class : Classes) {
        if (Class == WhichClass) {
          return true;
        }
      }
      return false;
    }
};

class AnyLoopCounter : public LoopClassifier {
  public:
    void classify(const NaturalLoop* Loop) const {
      Classifications[Loop->getUnsliced()].insert("ANY");
      LoopClassifier::classify(Loop, "Stmt", Loop->getLoopStmtMarker());
    }
};
