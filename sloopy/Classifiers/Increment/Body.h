#pragma once

/* BODY */

// TODO addPseudoConstantVar("i", Increment.VD, Increment.Statement);
// (i) denotes a line number in Aho 9.3.3, Fig. 9.23
/* NaturalLoop L = getLoop(LS, Context); */
/* if (L.Header) { */
/*   std::map<const CFGBlock*, unsigned> Out, In, OldOut; */

/*   for (auto B : L.Blocks) {   // (2) */
/*     Out[B] = 0; */
/*   } */
/*   unsigned n = 0; */
/*   for (auto Element : *L.Header) { */
/*     const Stmt* S = Element.castAs<CFGStmt>().getStmt(); */
/*     cloopy::PseudoConstantAnalysis A(S); */
/*     if (!A.isPseudoConstant(Increment.VD)) { */
/*       n++; */
/*     } */
/*   } */
/*   Out[L.Header] = n;          // (1) */
/*   while (Out != OldOut) {     // (3) */
/*     OldOut = Out; */

/*     for (auto B : L.Blocks) { // (4) */
/*       unsigned min = std::numeric_limits<unsigned int>::max(); */
/*       for (auto Pred = B->pred_begin(); Pred != B->pred_end(); Pred++) {  // (5) */
/*         if (L.Blocks.count(*Pred) == 0) continue;   // ignore preds outside the loop */
/*         if (Out[*Pred] < min) { */
/*           min = Out[*Pred]; */
/*         } */
/*       } */
/*       In[B] = min; */

/*       if (B != L.Header) { */
/*         // f_B(x) */
/*         unsigned n = 0; */
/*         for (auto Element : *B) { */
/*           const Stmt* S = Element.castAs<CFGStmt>().getStmt(); */
/*           cloopy::PseudoConstantAnalysis A(S); */
/*           if (!A.isPseudoConstant(Increment.VD)) { */
/*             n++; */
/*           } */
/*         } */
/*         Out[B] = std::min(2U, In[B] + n); // (6) */
/*       } */
/*     } */
/*   } */
/*   if (In[L.Header] < 1) */
/*       throw checkerror("!ADA_i-ASSIGNED-NotAllPaths"); */
/*   if (In[L.Header] > 1) */
/*       throw checkerror("!ADA_i-ASSIGNED-Twice"); */
/* } */
/* else { */
/*   std::cout<<"$$$IRRED?!$$$"<<std::endl; */
/* } */
