#pragma once

#include <sstream>
#include <algorithm>
#include <vector>
#include <stack>

#include "clang/AST/ASTContext.h"

using namespace clang;
using namespace clang::tooling;

class checkerror {
  const std::string Reason;
  public:
    checkerror(const std::string Reason) : Reason(Reason) {}

    const std::string what() const throw() {
      return Reason;
    }
};

/*
 * filename -line ... :
 *    -> Exits: 2
 *    -> Stmt: FOR
 *    -> SingleExit: (
 *         -> Simple: ""
 *       )
 */

//#define NUM_PROCS 5
#ifdef NUM_PROCS
class ProcessCounter {
  unsigned number_active = 0;
  std::condition_variable cv;

  template< class Predicate >
  void wait_while(Predicate Pred) {
    int status;
    while (Pred()) {
      wait(&status);
      if WIFEXITED(status) {
        --number_active;
      }
    }
  }

  public:
  void wait_alldone() {
    wait_while([this]{return number_active > 0;});
  }
  void wait_pool() {
    wait_while([this]{return number_active >= NUM_PROCS;});
    number_active++;
  }
};

std::mutex ClassificationsMutex;
ProcessCounter ProcCounter;
#define LOCK_GUARD std::lock_guard<std::mutex> guard(ClassificationsMutex);
#else
#define LOCK_GUARD
#endif

typedef std::map<std::string, boost::variant<int, unsigned, std::string>> IncrementClassificationValue;
typedef boost::variant<int, unsigned, std::string, IncrementClassificationValue> ClassificationValue;
typedef std::map<std::string, ClassificationValue> ClassificationProperty;
typedef std::map<const NaturalLoop * const, ClassificationProperty> ClassificationMap;
ClassificationMap Classifications;
std::map<const NaturalLoop*, std::string> LoopLocationMap;

enum class OutputFormat {
  JSON,
  Plain
};

class ClassificationValueVisitor : public boost::static_visitor<std::string> {
  const OutputFormat OF;
  public:
    ClassificationValueVisitor(const OutputFormat OF) : OF(OF) {}
    std::string operator()(int i) const {
      std::stringstream s;
      s << i;
      return " " + s.str();
    }
    std::string operator()(const std::string & str) const {
      if (OF == OutputFormat::JSON)
        return " \"" + str + "\"";
      return " " + str;
    }
    std::string operator()(const IncrementClassificationValue & V) const {
      std::stringstream sstm;
      if (OF == OutputFormat::JSON) sstm << "{\n";
      for (IncrementClassificationValue::const_iterator I = V.begin(),
                                                        E = V.end();
                                                        I != E; I++) {
        auto Value = *I;
        std::string result = boost::apply_visitor( ClassificationValueVisitor(OF), Value.second );
        sstm << "\t";
        if (OF == OutputFormat::JSON) sstm << "\"";
        sstm << Value.first;
        if (OF == OutputFormat::JSON) sstm << "\"";
        sstm << ": " << result;
        if (OF == OutputFormat::JSON && next(I) != E) sstm << ",\n";
      }
      if (OF == OutputFormat::JSON) sstm << "}\n";
      return "\n" + sstm.str();
    }
};

void dumpClasses(llvm::raw_ostream &out, const ClassificationProperty Property, const OutputFormat OF = OutputFormat::Plain) {
  for (ClassificationProperty::const_iterator I = Property.begin(),
                                              E = Property.end();
                                              I != E; I++) {
    auto Class = *I;
    if (OF == OutputFormat::JSON) out << "\"";
    out << Class.first;
    if (OF == OutputFormat::JSON) out << "\"";
    out << ":";
    std::string result = boost::apply_visitor( ClassificationValueVisitor(OF), Class.second );
    out << result;
    if (OF == OutputFormat::JSON && std::next(I) != E) out << ",";
    out << "\n";
  }
}

void dumpClasses(llvm::raw_ostream &out, const OutputFormat OF = OutputFormat::JSON) {
  if (OF == OutputFormat::JSON) out << "[\n";
  for (ClassificationMap::const_iterator I = Classifications.begin(),
                                       E = Classifications.end();
                                       I != E; I++) {
    auto Loop = I->first;
    auto Property = I->second;

    if (OF == OutputFormat::JSON) {
      out << "{\n";
      out << "\"Location\": \"" << LoopLocationMap[Loop] << "\",\n";
    } else {
      out << LoopLocationMap[Loop] << "\n";
    }
    dumpClasses(out, Property, OF);
    if (OF == OutputFormat::JSON) {
      out << "}\n";
      if (std::next(I) != E) {
        out << ",\n";
      }
    }
  }
  if (OF == OutputFormat::JSON) out << "]\n";
}


class LoopClassifier {
  public:
    static void classify(const NaturalLoop* Loop, const std::string Property) {
      LOCK_GUARD
      Classifications[Loop->getUnsliced()][Property] = 1;
    }
    template<typename T>
    static void classify(const NaturalLoop* Loop, const std::string Property, const T Value) {
      LOCK_GUARD
      Classifications[Loop->getUnsliced()][Property] = Value;
    }
    static void classify(const NaturalLoop* Loop, const std::string SubClass, const std::string Property, const std::string Value, const bool Success=true) {
      std::stringstream sstm;
      sstm << (Success ? "" : "!");
      sstm << Value;
      LOCK_GUARD
      auto &C = Classifications[Loop->getUnsliced()];
      if (C.find(SubClass) == C.end()) {
        C[SubClass] = IncrementClassificationValue();
      }
      auto &ICV = boost::get<IncrementClassificationValue>(C[SubClass]);
      ICV[Property] = sstm.str();
    }
    template<typename T>
    static void classify(const NaturalLoop* Loop, const std::string SubClass, const std::string Property, const T Value) {
      LOCK_GUARD
      auto &C = Classifications[Loop->getUnsliced()];
      if (C.find(SubClass) == C.end()) {
        C[SubClass] = IncrementClassificationValue();
      }
      auto &ICV = boost::get<IncrementClassificationValue>(C[SubClass]);
      ICV[Property] = Value;
    }
    static bool hasClass(const NaturalLoop* Loop, const std::string Property) {
      LOCK_GUARD
      auto C = Classifications[Loop->getUnsliced()][Property];
      int I = boost::get<int>(C);
      return I;
    }
};

class AnyLoopCounter : public LoopClassifier {
  public:
    void classify(const NaturalLoop* Loop) const {
      LoopClassifier::classify(Loop, "ANY");
      LoopClassifier::classify(Loop, "Stmt", Loop->getLoopStmtMarker());
    }
};
