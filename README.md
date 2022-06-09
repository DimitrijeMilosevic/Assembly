# Overview

Assembly parses a source file (written in *x86*-based assembly language) in one pass by using backtracking to resolve unknown addresses. It generates an object file with *x86* directives and utility data structures (i.e. symbol table, list of exported and imported names, etc.), which can be used during linking and loading.
