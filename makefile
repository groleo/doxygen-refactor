LLVM_VERSION=3.8
LLVM_BIN_PATH := /usr/lib/llvm-$(LLVM_VERSION)/bin/

# CXX has to be a fairly modern C++ compiler that supports C++11.
#
# gcc 4.9 and higher or Clang 3.8 and higher are recommended.
# If you build LLVM from sources, use the same compiler you built LLVM with.
CXX := /usr/bin/clang++-$(LLVM_VERSION)
CXXFLAGS :=
LLVM_CXXFLAGS := `$(LLVM_BIN_PATH)/llvm-config --cxxflags`
LLVM_LDFLAGS := -v `$(LLVM_BIN_PATH)/llvm-config --ldflags --libs --system-libs`

CLANG_INCLUDES := \
	-I/usr/lib/llvm-$(LLVM_VERSION)/lib/clang/$(LLVM_VERSION).0/include/

# List of Clang libraries to link. The proper -L will be provided by the
# call to llvm-config
# Note that I'm using -Wl,--{start|end}-group around the Clang libs; this is
# because there are circular dependencies that make the correct order difficult
# to specify and maintain. The linker group options make the linking somewhat
# slower, but IMHO they're still perfectly fine for tools that link with Clang.



CLANG_LIBS := \
	-Wl,--start-group \
	-lclang		\
	-lclangAST	\
	-lclangAnalysis	\
	-lclangBasic	\
	-lclangDriver	\
	-lclangEdit	\
	-lclangFrontend	\
	-lclangFrontendTool	\
	-lclangLex		\
	-lclangSema		\
	-lclangEdit		\
	-lclangASTMatchers	\
	-lclangRewrite		\
	-lclangRewriteFrontend	\
	-lclangStaticAnalyzerFrontend \
	-lclangStaticAnalyzerCheckers \
	-lclangStaticAnalyzerCore \
	-lclangSerialization	\
	-lclangTooling		\
	-lclangToolingCore	\
	-lclangFrontendTool	\
	-lclangParse	\
	-Wl,--end-group

DOXYGEN_DIR=$(shell realpath ../doxygen)
REF_SRC=$(PWD)

.PHONY: all
all: r s

refactor: refactor.cpp
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(CLANG_INCLUDES) refactor.cpp -c -o refactor.o
	$(CXX) refactor.o $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

.PHONY: test
test:
	$(MAKE) -C test

.PHONY: r
r: refactor
	mkdir -p $(DOXYGEN_DIR)/build  && \
	cd $(DOXYGEN_DIR)              && \
	git reset --hard               && \
	grep -rI MemberNameInfoIterator . | cut -f1 -d: | sort | uniq | xargs sed -e 's/MemberNameInfoIterator/MemberInfoListIterator/g' -i &&\
	grep -rI MemberNameInfo . | cut -f1 -d: | sort | uniq | xargs sed -e 's/MemberNameInfo/MemberInfoList/g' -i &&\
	cd -                           && \
	cd $(DOXYGEN_DIR)/build        && \
	cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:STRING=ON .. && \
	make
	ulimit -c unlimited            && \
	$< $(DOXYGEN_DIR)/build/ $(DOXYGEN_DIR)/src/*.cpp -- -I$(DOXYGEN_DIR)/libmd5 -I$(DOXYGEN_DIR)/qtools -I$(DOXYGEN_DIR)/build/generated_src/ -I$(DOXYGEN_DIR)/src/ -I$(DOXYGEN_DIR)/vhdlparser/ $(CLANG_INCLUDES)
	cd $(DOXYGEN_DIR)/qtools       && \
	git checkout -- .              && \
	cd $(DOXYGEN_DIR)              && \
	git status -s | sed -n 's/^ M //p' | xargs sed -e '0,/#include .*/s//#include <list>\n#include <memory>\n&/' -i
	cd $(DOXYGEN_DIR)              && \
	for file in `git status -s | sed -n 's/^ M //p' `; do \
	cat $${file} | $(REF_SRC)/bexy.pl > $${file}.ref ; \
	mv $${file}.ref $${file} ; done


.PHONY: q
q:
	cd $(DOXYGEN_DIR)/build && git reset --hard
	clang-query-$(LLVM_VERSION) -p $(DOXYGEN_DIR)/build/ $(DOXYGEN_DIR)/src/*.cpp $(DOXYGEN_DIR)/src/*.h

.PHONY: clean
clean:
	rm -rf refactor refactor.o core *.dot test/*.pyc test/__pycache__


.PHONY: help
help:
	@echo "r - refactor"
	@echo "s - update status in README.md"
	@echo "q - run clang-query"

s:
	sed -e '/Refactorings status/,$$ d' -i README.md
	echo 'Refactorings status' >> README.md
	echo '-------------------' >> README.md
	echo >> README.md
	grep -r 'O:' refactor.cpp | cut -f2- -d: >> README.md

