CMAKE_BUILD_FOLDER = build
CLANG_CACHE_FOLDER = .cache
THIRD_PARTY_FOLDER = third_party
TOOLS_FOLDER = tools
ASSET_FOLDER = assets
STATE_FOLDER = state

EXECUTABLE_NAME = ouro
EXECUTABLE_PATH = ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}
INSTALL_PATH = ~/Games/${EXECUTABLE_NAME}

CC = clang
CXX = clang++
CMAKE_GENERATOR = Ninja
NUM_CORES = ${shell nproc --all}

OPTIONS_FILE = options.uft
DEBUG_FILE = debug.uft

BUILD_TYPE = Debug

BUILD_FLAGS = \
	-DUSE_AUDIO=OFF \
	-DWITH_PIC=ON \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DCUSTOMIZE_BUILD=ON \
	-DPLATFORM=Desktop \
	-DGRAPHICS=GRAPHICS_API_OPENGL_43 \
	-DSUPPORT_MODULE_RAUDIO=OFF \
	-DSUPPORT_FILEFORMAT_JPG=ON \
	-DSUPPORT_FILEFORMAT_HDR=ON \
	-DSUPPORT_FILEFORMAT_TGA=ON \
	-DSUPPORT_DEFAULT_FONT=OFF \
	-DSUPPORT_SCREEN_CAPTURE=OFF \
	-DSUPPORT_CLIPBOARD_IMAGE=OFF \
	-DSUPPORT_GESTURES_SYSTEM=OFF \
	-DSUPPORT_MOUSE_GESTURES=OFF \

CCACHE_FLAGS = \
	-DCMAKE_C_COMPILER_LAUNCHER=ccache \
	-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \

ALL_PROJECT_FILES := $(shell find src \( -name "*.cpp" -o -name "*.hpp" \))
ALL_SHADER_FILES := $(shell find assets/shaders -type f -name "*.glsl")
DIALOGUE_PARSER_SOURCES := $(shell find tools/dialogue_parser -name "*.go")
CVAR_PARSER_SOURCES := $(shell find tools/cvar_parser -name "*.go")

DIALOGUE_SRC_FOLDER = dialogues
DIALOGUE_DST_FOLDER = src/dialogues
DIALOGUE_CACHE_FOLDER = build/dialogues/

CVAR_SRC_FILE = ouro.cvar.default
CVAR_DST_FILE = src/cvar

DIALOGUE_PARSER_BIN = ${CMAKE_BUILD_FOLDER}/dialogue_parser
CVAR_PARSER_BIN = ${CMAKE_BUILD_FOLDER}/cvar_parser

COLORED = \033[0;36m
NC = \033[0m

.PHONY: all
all: init build

.PHONY: init
init: ${CMAKE_BUILD_FOLDER}/CMakeCache.txt ## init the whole project (excluding third_party)

${CMAKE_BUILD_FOLDER}/CMakeCache.txt:
	CC=${CC} CXX=${CXX} cmake -S . -B ${CMAKE_BUILD_FOLDER} -G "${CMAKE_GENERATOR}" -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${CMAKE_FLAGS} ${BUILD_FLAGS} ${CCACHE_FLAGS}

.PHONY: build
build: init build-dialogues build-cvars ## build the project (use BUILD_TYPE=Release/Debug if needed)
	@echo -e "${COLORED}# Building with ${NUM_CORES} cores${NC}"
	cmake --build ${CMAKE_BUILD_FOLDER} --parallel ${NUM_CORES}

${DIALOGUE_PARSER_BIN}: ${DIALOGUE_PARSER_SOURCES}
	@echo -e "${COLORED}# Building dialogue parser${NC}"
	cd tools/dialogue_parser && go build -o $(CURDIR)/${DIALOGUE_PARSER_BIN} .

${CVAR_PARSER_BIN}: ${CVAR_PARSER_SOURCES}
	@echo -e "${COLORED}# Building cvar parser${NC}"
	cd tools/cvar_parser && go build -o $(CURDIR)/${CVAR_PARSER_BIN} .

.PHONY: build-dialogues
build-dialogues: ${DIALOGUE_PARSER_BIN} ## build the dialogues for the project
	@echo -e "${COLORED}# Building dialogues${NC}"
	cd tools/dialogue_parser && $(CURDIR)/${DIALOGUE_PARSER_BIN} $(CURDIR)/${DIALOGUE_SRC_FOLDER} $(CURDIR)/${DIALOGUE_DST_FOLDER} $(CURDIR)/${DIALOGUE_CACHE_FOLDER}

.PHONY: build-cvars
build-cvars: ${CVAR_PARSER_BIN} ## build the cvars for the project
	@echo -e "${COLORED}# Building CVars${NC}"
	cd tools/cvar_parser && $(CURDIR)/${CVAR_PARSER_BIN} $(CURDIR)/${CVAR_SRC_FILE} $(CURDIR)/${CVAR_DST_FILE}

.PHONY: run
run: build ## run the project
	@echo -e "${COLORED}# Running ${EXECUTABLE_NAME}${NC}"
	@PLATFORM=$$(${TOOLS_FOLDER}/detect_platform.sh); \
	${EXECUTABLE_PATH} --platform $$PLATFORM

.PHONY: emacs-run
emacs-run: build ## run the project from emacs
	@echo -e "${COLORED}# Running ${EXECUTABLE_NAME}${NC}"
	@PLATFORM=$$(${TOOLS_FOLDER}/detect_platform.sh); \
	${EXECUTABLE_PATH} --emacs --platform $$PLATFORM

.PHONY: test
test: build ## test the project
	@echo -e "${COLORED}# Running tests${NC}"
	${EXECUTABLE_PATH} --test

.PHONY: clean
clean: ## clean the project
	@echo -e "${COLORED}# Cleaning${NC}"
	rm -rf ${CMAKE_BUILD_FOLDER}
	rm -rf ${CLANG_CACHE_FOLDER}
	rm -rf ${DIALOGUE_DST_FOLDER}

.PHONY: clean-full
clean-full: ## clean the project including ccache
	@echo -e "${COLORED}# Full cleaning (including ccache)${NC}"
	ccache --clear -z
	rm -rf ${CMAKE_BUILD_FOLDER}
	rm -rf ${CLANG_CACHE_FOLDER}
	rm -rf ${DIALOGUE_DST_FOLDER}

.PHONY: install
install: clean ## install the project
	@echo -e "${COLORED}# Installing${NC}"
	make BUILD_TYPE=Release build
	strip ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}
	if [ ! -d "${INSTALL_PATH}" ]; then mkdir -p ${INSTALL_PATH}; fi
	cp ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME} ${INSTALL_PATH}
	cp -r ${ASSET_FOLDER} ${INSTALL_PATH}

.PHONY: install-deck
install-deck: clean ## install on Steam Deck remotely
	@echo -e "${COLORED}# Installing on Steam Deck${NC}"
	./tools/install_on_deck.sh

.PHONY: debug
debug: build ## debug the project with GDB
	@echo -e "${COLORED}# Debugging with GDB${NC}"
	gdb --ex "run" -q --args ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME} --debugger

.PHONY: debug-gf2
debug-gf2: build ## debug the project with gf2
	@echo -e "${COLORED}# Debugging with gf2${NC}"
	gf2 --ex "run" -q --args ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME} --debugger

.PHONY: format-shaders
format-shaders: ## format shader files
	@echo -e "${COLORED}# Formatting shaders${NC}"
	clang-format -i -style=file ${ALL_SHADER_FILES}

.PHONY: check
check: build ## check the project via static linters and tools
	@echo -e "${COLORED}# Checking${NC}"
	go run ${TOOLS_FOLDER}/stupid/main.go ./src
	cppcheck --std=c++20 --enable=warning --suppress=internalAstError --suppress=variableScope --suppress=passedByValue --suppress=useStlAlgorithm --suppress=deallocret --suppress=cstyleCast --suppress=unreadVariable --suppress=unknownEvaluationOrder --suppress=constParameterPointer --suppress=constParameterCallback --suppress=unusedPrivateFunction --suppress=unknownMacro --suppress=unusedStructMember --suppress=constVariablePointer --suppress=dangerousTypeCast --enable=performance,portability,style ${ALL_PROJECT_FILES} --quiet -j ${NUM_CORES} --check-level=exhaustive
	run-clang-tidy -p $(CMAKE_BUILD_FOLDER) -header-filter='^src/' $(ALL_PROJECT_FILES) -quiet -use-color

.PHONY: valgrind-leaks
valgrind-leaks: build ## detect memory leaks
	@echo -e "${COLORED}# Running Valgrind Memcheck (memory leaks)${NC}"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}

.PHONY: valgrind-cache
valgrind-cache: build ## analyze CPU cache performance
	@echo -e "${COLORED}# Running Valgrind Cachegrind (cache profiling)${NC}"
	valgrind --tool=cachegrind --cachegrind-out-file=${CMAKE_BUILD_FOLDER}/cachegrind.out.%p ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}
	@echo -e "${COLORED}# View results with: cg_annotate ${CMAKE_BUILD_FOLDER}/cachegrind.out.<pid>${NC}"

.PHONY: valgrind-callgraph
valgrind-callgraph: build ## profile with call graphs
	@echo -e "${COLORED}# Running Valgrind Callgrind (call-graph profiling)${NC}"
	valgrind --tool=callgrind --callgrind-out-file=${CMAKE_BUILD_FOLDER}/callgrind.out.%p ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}
	@echo -e "${COLORED}# View results with: callgrind_annotate ${CMAKE_BUILD_FOLDER}/callgrind.out.<pid>${NC}"
	@echo -e "${COLORED}# Or use GUI with: kcachegrind ${CMAKE_BUILD_FOLDER}/callgrind.out.<pid>${NC}"

.PHONY: valgrind-heap
valgrind-heap: build ## track heap memory usage over time
	@echo -e "${COLORED}# Running Valgrind Massif (heap profiling)${NC}"
	valgrind --tool=massif --massif-out-file=${CMAKE_BUILD_FOLDER}/massif.out.%p ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}
	@echo -e "${COLORED}# View results with: ms_print ${CMAKE_BUILD_FOLDER}/massif.out.<pid>${NC}"

.PHONY: valgrind-races
valgrind-races: build ## detect thread race conditions
	@echo -e "${COLORED}# Running Valgrind Helgrind (thread race detection)${NC}"
	valgrind --tool=helgrind ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}

.PHONY: valgrind-races-fast
valgrind-races-fast: build ## detect thread races (faster)
	@echo -e "${COLORED}# Running Valgrind DRD (thread race detection - fast)${NC}"
	valgrind --tool=drd ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}

.PHONY: valgrind-heap-analysis
valgrind-heap-analysis: build ## analyze heap allocation patterns
	@echo -e "${COLORED}# Running Valgrind DHAT (heap analysis)${NC}"
	valgrind --tool=dhat --dhat-out-file=${CMAKE_BUILD_FOLDER}/dhat.out.%p ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}
	@echo -e "${COLORED}# View results by opening ${CMAKE_BUILD_FOLDER}/dhat.out.<pid> in a web browser${NC}"

.PHONY: profile
profile: build ## record performance with perf and open the report automatically
	@echo -e "${COLORED}# Profiling with perf... (Quit the app to see the report)${NC}"
	perf record -g -o ${CMAKE_BUILD_FOLDER}/perf.data ${EXECUTABLE_PATH} && perf report -i ${CMAKE_BUILD_FOLDER}/perf.data

.PHONY: profile-gui
profile-gui: build ## profile with perf and view in Hotspot
	@echo -e "${COLORED}# Profiling with perf... (Quit the app to see the report)${NC}"
	perf record -g -o ${CMAKE_BUILD_FOLDER}/perf.data ${EXECUTABLE_PATH} && QT_SCALE_FACTOR=2.0 hotspot ${CMAKE_BUILD_FOLDER}/perf.data

.PHONY: deps
deps: ## redownload third party libraries
	@echo -e "${COLORED}# Re-populating third party${NC}"
	$(TOOLS_FOLDER)/setup_deps.sh --force ${THIRD_PARTY_FOLDER}

.PHONY: help
help: ## print this help
	@echo -e "${COLORED}# Available targets${NC}"
	@grep -E -h '\s##\s' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[33m%-26s\033[0m %s\n", $$1, $$2}'
