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
	-DSUPPORT_GIF_RECORDING=OFF \
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

CVAR_SRC_FILE = ouro.cvar
CVAR_DST_FILE = src/cvar

DIALOGUE_PARSER_BIN = ${CMAKE_BUILD_FOLDER}/dialogue_parser
CVAR_PARSER_BIN = ${CMAKE_BUILD_FOLDER}/cvar_parser

COLORED = \033[0;36m
NC = \033[0m

.PHONY: all init build build-dialogues build-cvars run emacs-run test clean install debug debug-gf2 format check valgrind profile profile-gui deps help

all: init build

init: ${CMAKE_BUILD_FOLDER}/CMakeCache.txt ## init the whole project (excluding third_party)

${CMAKE_BUILD_FOLDER}/CMakeCache.txt:
	CC=${CC} CXX=${CXX} cmake -S . -B ${CMAKE_BUILD_FOLDER} -G "${CMAKE_GENERATOR}" -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${CMAKE_FLAGS} ${BUILD_FLAGS} ${CCACHE_FLAGS}

build: init build-dialogues build-cvars ## build the project (use BUILD_TYPE=Release/Debug if needed)
	@echo -e "${COLORED}# Building with ${NUM_CORES} cores${NC}"
	cmake --build ${CMAKE_BUILD_FOLDER} --parallel ${NUM_CORES}

${DIALOGUE_PARSER_BIN}: ${DIALOGUE_PARSER_SOURCES}
	@echo -e "${COLORED}# Building dialogue parser${NC}"
	cd tools/dialogue_parser && go build -o $(CURDIR)/${DIALOGUE_PARSER_BIN} .

${CVAR_PARSER_BIN}: ${CVAR_PARSER_SOURCES}
	@echo -e "${COLORED}# Building cvar parser${NC}"
	cd tools/cvar_parser && go build -o $(CURDIR)/${CVAR_PARSER_BIN} .

build-dialogues: ${DIALOGUE_PARSER_BIN} ## build the dialogues for the project
	@echo -e "${COLORED}# Building dialogues${NC}"
	cd tools/dialogue_parser && $(CURDIR)/${DIALOGUE_PARSER_BIN} $(CURDIR)/${DIALOGUE_SRC_FOLDER} $(CURDIR)/${DIALOGUE_DST_FOLDER} $(CURDIR)/${DIALOGUE_CACHE_FOLDER}

build-cvars: ${CVAR_PARSER_BIN} ## build the cvars for the project
	@echo -e "${COLORED}# Building CVars${NC}"
	cd tools/cvar_parser && $(CURDIR)/${CVAR_PARSER_BIN} $(CURDIR)/${CVAR_SRC_FILE} $(CURDIR)/${CVAR_DST_FILE}

run: build ## run the project
	@echo -e "${COLORED}# Running ${EXECUTABLE_NAME}${NC}"
	${EXECUTABLE_PATH}

emacs-run: build ## run the project with a preset configuration for running it from emacs
	@echo -e "${COLORED}# Running ${EXECUTABLE_NAME}${NC}"
	${EXECUTABLE_PATH} --emacs

test: build ## test the project
	@echo -e "${COLORED}# Running tests${NC}"
	${EXECUTABLE_PATH} --test

clean: ## clean the project
	@echo -e "${COLORED}# Cleaning${NC}"
	# ccache --clear -z
	rm -rf ${CMAKE_BUILD_FOLDER}
	rm -rf ${CLANG_CACHE_FOLDER}
	rm -rf ${DIALOGUE_DST_FOLDER}

install: clean ## install the project
	@echo -e "${COLORED}# Installing${NC}"
	make BUILD_TYPE=Release build
	strip ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}
	if [ ! -d "${INSTALL_PATH}" ]; then mkdir -p ${INSTALL_PATH}; fi
	cp ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME} ${INSTALL_PATH}
	cp -r ${ASSET_FOLDER} ${INSTALL_PATH}

install-deck: clean ## install remotely on the deck
	@echo -e "${COLORED}# Installing on Steam Deck${NC}"
	./tools/install_on_deck.sh

debug: build ## debug the project
	@echo -e "${COLORED}# Debugging${NC}"
	gdb --ex "run" -q --args ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME} --debugger

debug-gf2: build ## debug the project
	@echo -e "${COLORED}# Debugging${NC}"
	gf2 --ex "run" -q --args ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME} --debugger

format: ## format the project
	@echo -e "${COLORED}# Formatting${NC}"
# clang-format -i -style=file ${ALL_PROJECT_FILES}
	clang-format -i -style=file ${ALL_SHADER_FILES}

check: build ## check the project via static linters and tools
	@echo -e "${COLORED}# Checking${NC}"
	go run ${TOOLS_FOLDER}/stupid/main.go ./src
	cppcheck --std=c++20 --enable=warning --suppress=internalAstError --suppress=variableScope --suppress=passedByValue --suppress=useStlAlgorithm --suppress=deallocret --suppress=cstyleCast --suppress=unknownEvaluationOrder --suppress=constParameterPointer --suppress=constParameterCallback --suppress=unusedPrivateFunction --suppress=unknownMacro --suppress=unusedStructMember --suppress=constVariablePointer --suppress=dangerousTypeCast --enable=performance,portability,style ${ALL_PROJECT_FILES} --quiet -j ${NUM_CORES} --check-level=exhaustive
	run-clang-tidy -p $(CMAKE_BUILD_FOLDER) -header-filter='^src/' $(ALL_PROJECT_FILES) -quiet -use-color

valgrind: build ## valgrind the project
	@echo -e "${COLORED}# Running Valgrind${NC}"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ${CMAKE_BUILD_FOLDER}/${EXECUTABLE_NAME}

profile: build ## record performance with perf and open the report automatically
	@echo -e "${COLORED}# Profiling with perf... (Quit the app to see the report)${NC}"
	perf record -g -o ${CMAKE_BUILD_FOLDER}/perf.data ${EXECUTABLE_PATH} && perf report -i ${CMAKE_BUILD_FOLDER}/perf.data

profile-gui: build ## Profile with perf and view in Hotspot
	@echo -e "${COLORED}# Profiling with perf... (Quit the app to see the report)${NC}"
	perf record -g -o ${CMAKE_BUILD_FOLDER}/perf.data ${EXECUTABLE_PATH} && QT_SCALE_FACTOR=2.0 hotspot ${CMAKE_BUILD_FOLDER}/perf.data

deps: ## redownload third party libraries
	@echo -e "${COLORED}# Re-populating third party${NC}"
	$(TOOLS_FOLDER)/setup-deps.sh --force ${THIRD_PARTY_FOLDER}

help: ## print this help
	@grep -E -h '\s##\s' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'
